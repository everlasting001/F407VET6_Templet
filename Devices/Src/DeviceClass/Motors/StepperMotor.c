/**
  ******************************************************************************
  * @file    StepperMotor.c
  * @brief   步进电机子类实现 — 28BYJ-48 步进控制 (DWT 微秒级梯形加减速)
  *
  * @details
  * 提供 StepperMotor_t 的完整实现，包括：
  *   - 虚函数表 (init / run / cleanup / reset)
  *   - 3 种步进模式拍序表
  *   - DWT 周期计数器驱动的微秒级梯形加减速
  *   - 角度/脉冲双跟踪
  *
  * === 转速参考 (28BYJ-48 实践验证) ===
  *
  * | 等级  | 步间间隔 | 脉冲频率 | 4096步/转耗时 |
  * |-------|---------|---------|-------------|
  * |  1    | 1500us  |  667pps | ~6.1s/rev   |
  * |  5    |  800us  | 1250pps | ~3.3s/rev   |
  * | 10    |  500us  | 2000pps | ~2.0s/rev   |
  *
  * === DWT 非阻塞时序 ===
  *
  * run() 在主循环中通过 DWT_ElapsedUs() 检查距离上一步的经过时间，
  * 达到 step_interval_us 后执行一步。无 ISR 依赖，微秒级精度。
  *
  * === 梯形加减速 ===
  *
  * 加速段：步间间隔从 start_interval 线性递减到 cruise_interval
  * 匀速段：步间间隔保持 cruise_interval
  * 减速段：步间间隔从 cruise_interval 线性递增到 start_interval
  * 短行程自动跳过匀速段，极短行程跳过梯形直接匀速。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "StepperMotor.h"
#include "dwt_delay.h"
#include "stm32f4xx_hal.h"

/* ==================== 私有宏 ==================== */

#define STEPPER_PHASE_COUNT       4U     /**< 四相步进电机 */
#define HALF_CYCLE_STEPS          8U     /**< 半步八拍每周期步数 */
#define FULL_CYCLE_STEPS          4U     /**< 整步/波驱动每周期步数 */
#define MAX_START_INTERVAL_US  3000U     /**< 加速起始间隔上限 (us) */
#define ACCEL_RATIO_NUM           1U     /**< 加速段占比分子 */
#define ACCEL_RATIO_DEN           3U     /**< 加速段占比分母 */
#define ACCEL_START_MULT_NUM      2U     /**< 加速起始 = 巡航 * 2, 分母=1 */
#define ACCEL_START_MULT_DEN      1U     /**< 保持简单整数比 */

/* ==================== 步进拍序表 ==================== */

static const uint8_t half_step_seq[HALF_CYCLE_STEPS][STEPPER_PHASE_COUNT] = {
    {1, 0, 0, 0},  /* Step 0: A         */
    {1, 1, 0, 0},  /* Step 1: A+B       */
    {0, 1, 0, 0},  /* Step 2: B         */
    {0, 1, 1, 0},  /* Step 3: B+C       */
    {0, 0, 1, 0},  /* Step 4: C         */
    {0, 0, 1, 1},  /* Step 5: C+D       */
    {0, 0, 0, 1},  /* Step 6: D         */
    {1, 0, 0, 1},  /* Step 7: D+A       */
};

static const uint8_t full_step_seq[FULL_CYCLE_STEPS][STEPPER_PHASE_COUNT] = {
    {1, 1, 0, 0},  /* Step 0: A+B */
    {0, 1, 1, 0},  /* Step 1: B+C */
    {0, 0, 1, 1},  /* Step 2: C+D */
    {1, 0, 0, 1},  /* Step 3: D+A */
};

static const uint8_t wave_step_seq[FULL_CYCLE_STEPS][STEPPER_PHASE_COUNT] = {
    {1, 0, 0, 0},  /* Step 0: A */
    {0, 1, 0, 0},  /* Step 1: B */
    {0, 0, 1, 0},  /* Step 2: C */
    {0, 0, 0, 1},  /* Step 3: D */
};

/* ==================== 转速等级 → 巡航间隔映射 (us) ==================== */

/**
  * @brief 转速等级对应的巡航步间间隔 (us)
  * @note  参考 28BYJ-48 实践验证数据:
  *        SLOW=1500us, MEDIUM=800us, FAST=500us
  */
static const uint16_t speed_interval_table[10] = {
    1500,  /* Grade 1:   667 pps (SLOW)   */
    1200,  /* Grade 2:   833 pps          */
    1000,  /* Grade 3:  1000 pps          */
     900,  /* Grade 4:  1111 pps          */
     800,  /* Grade 5:  1250 pps (MEDIUM, 默认) */
     700,  /* Grade 6:  1429 pps          */
     600,  /* Grade 7:  1667 pps          */
     550,  /* Grade 8:  1818 pps          */
     500,  /* Grade 9:  2000 pps (FAST)   */
     500,  /* Grade 10: 2000 pps (TIM2 1ms limit无关, DWT无此限制) */
};

/* ==================== 私有辅助函数 ==================== */

static uint8_t stepper_get_cycle_steps(StepperStepMode mode)
{
    return (mode == STEP_MODE_HALF_8) ? HALF_CYCLE_STEPS : FULL_CYCLE_STEPS;
}

static uint16_t stepper_get_steps_per_rev(StepperStepMode mode)
{
    return (mode == STEP_MODE_HALF_8)
           ? STEPPER_STEPS_PER_REV_HALF
           : (STEPPER_STEPS_PER_REV_HALF / 2);
}

static const uint8_t (*stepper_get_sequence(StepperStepMode mode))[STEPPER_PHASE_COUNT]
{
    switch (mode) {
        case STEP_MODE_FULL_4:
            return full_step_seq;
        case STEP_MODE_WAVE_4:
            return wave_step_seq;
        case STEP_MODE_HALF_8:
        default:
            return half_step_seq;
    }
}

static void stepper_write_phases(const StepperMotor_t *motor, uint8_t step_index)
{
    const uint8_t (*seq)[STEPPER_PHASE_COUNT] = stepper_get_sequence(motor->step_mode);
    uint8_t cycle_steps = stepper_get_cycle_steps(motor->step_mode);

    for (uint8_t i = 0; i < STEPPER_PHASE_COUNT; i++) {
        GPIO_PinState state = (seq[step_index % cycle_steps][i])
                              ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin, state);
    }
}

static int32_t pulses_to_angle(int32_t pulses, StepperStepMode mode)
{
    uint16_t steps_per_rev = stepper_get_steps_per_rev(mode);
    return (int32_t)(((int64_t)pulses * STEPPER_DEGREES_PER_REV
                      * STEPPER_ANGLE_SCALE) / steps_per_rev);
}

static int32_t angle_to_pulses(int32_t angle_centideg, StepperStepMode mode)
{
    uint16_t steps_per_rev = stepper_get_steps_per_rev(mode);
    return (int32_t)(((int64_t)angle_centideg * steps_per_rev)
                     / (STEPPER_DEGREES_PER_REV * STEPPER_ANGLE_SCALE));
}

static void stepper_calc_trapezoid(StepperMotor_t *motor, uint32_t total)
{
    motor->total_steps = total;

    if (total <= 2) {
        motor->accel_steps   = total;
        motor->cruise_steps  = 0;
        motor->decel_steps   = 0;
    } else {
        uint32_t ramp = total / ACCEL_RATIO_DEN;
        if (ramp < 1) ramp = 1;

        motor->accel_steps = ramp;
        motor->decel_steps = ramp;

        if (motor->accel_steps + motor->decel_steps >= total) {
            motor->accel_steps  = total / 2;
            motor->decel_steps  = total - motor->accel_steps;
            motor->cruise_steps = 0;
        } else {
            motor->cruise_steps = total - motor->accel_steps - motor->decel_steps;
        }
    }
}

static uint32_t stepper_get_cruise_interval_us(uint8_t grade)
{
    if (grade < 1)  grade = 1;
    if (grade > 10) grade = 10;
    return (uint32_t)speed_interval_table[grade - 1];
}

static void stepper_start_trapezoid(StepperMotor_t *motor)
{
    uint32_t cruise_us = stepper_get_cruise_interval_us(motor->speed_grade);

    /* 加速起始间隔 = cruise * 2, 上限 MAX_START_INTERVAL_US */
    uint32_t start_us = (cruise_us * ACCEL_START_MULT_NUM) / ACCEL_START_MULT_DEN;
    if (start_us > MAX_START_INTERVAL_US) {
        start_us = MAX_START_INTERVAL_US;
    }

    motor->step_interval_us = start_us;
    motor->last_step_tick   = DWT_GetTick_us();
    motor->phase_step_count = 0;
    motor->trap_phase       = TRAPEZOID_ACCEL;
    motor->base.state       = MOTOR_STATE_RUNNING;
}

static void stepper_update_trapezoid(StepperMotor_t *motor)
{
    uint32_t cruise_us = stepper_get_cruise_interval_us(motor->speed_grade);
    uint32_t start_us  = (cruise_us * ACCEL_START_MULT_NUM) / ACCEL_START_MULT_DEN;
    if (start_us > MAX_START_INTERVAL_US) {
        start_us = MAX_START_INTERVAL_US;
    }

    uint32_t steps_taken = motor->phase_step_count;
    uint32_t interval_us;
    uint32_t step_in_phase;
    int32_t  diff;

    switch (motor->trap_phase) {
        case TRAPEZOID_ACCEL:
            if (motor->accel_steps > 0) {
                step_in_phase = steps_taken;
                diff = (int32_t)(start_us - cruise_us);
                interval_us = start_us
                    - (uint32_t)((diff * (int32_t)step_in_phase)
                                 / (int32_t)motor->accel_steps);
            } else {
                interval_us = cruise_us;
            }
            break;

        case TRAPEZOID_CRUISE:
            interval_us = cruise_us;
            break;

        case TRAPEZOID_DECEL:
            if (motor->decel_steps > 0) {
                step_in_phase = steps_taken
                    - motor->accel_steps
                    - motor->cruise_steps;
                diff = (int32_t)(start_us - cruise_us);
                interval_us = cruise_us
                    + (uint32_t)((diff * (int32_t)step_in_phase)
                                 / (int32_t)motor->decel_steps);
            } else {
                interval_us = cruise_us;
            }
            break;

        case TRAPEZOID_DONE:
        default:
            interval_us = cruise_us;
            break;
    }

    /* 确保不低于巡航速度，不低于 1us */
    if (interval_us < cruise_us) {
        interval_us = cruise_us;
    }
    if (interval_us < 1) {
        interval_us = 1;
    }
    motor->step_interval_us = interval_us;
}

/* ==================== 虚函数实现 ==================== */

static int StepperMotor_init(void *self)
{
    StepperMotor_t *motor = (StepperMotor_t *)self;

    if (motor->pins == NULL) {
        return -1;
    }

    for (uint8_t i = 0; i < STEPPER_PHASE_COUNT; i++) {
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    motor->step_index       = 0;
    motor->current_pulses   = 0;
    motor->current_angle    = 0;
    motor->target_angle     = 0;
    motor->target_pulses    = 0;
    motor->step_interval_us = speed_interval_table[4];  /* grade 5 default */
    motor->last_step_tick   = 0;
    motor->phase_step_count = 0;
    motor->trap_phase       = TRAPEZOID_DONE;
    motor->direction        = 0;

    return 0;
}

/**
  * @brief  步进电机 run 虚函数 — DWT 非阻塞步进
  * @note   在主循环中调用。通过 DWT 周期计数器检查是否到达步进时刻。
  *         无 ISR 依赖，无阻塞等待，微秒级精度。
  */
static int StepperMotor_run(void *self)
{
    StepperMotor_t *motor = (StepperMotor_t *)self;

    if (motor->base.state != MOTOR_STATE_RUNNING) {
        return 0;
    }

    /* 检查是否到达步进时刻 */
    uint32_t elapsed = DWT_ElapsedUs(motor->last_step_tick);
    if (elapsed < motor->step_interval_us) {
        return 0;
    }

    /* === 写入当前拍序 === */
    uint8_t cycle_steps = stepper_get_cycle_steps(motor->step_mode);
    stepper_write_phases(motor, motor->step_index);

    /* === 更新时间戳 (在步进之后, 确保间隔从此刻开始) === */
    motor->last_step_tick = DWT_GetTick_us();

    /* === 更新步序索引 === */
    if (motor->direction == 0) {
        motor->step_index = (motor->step_index + 1) % cycle_steps;
        motor->current_pulses++;
    } else {
        motor->step_index = (motor->step_index + cycle_steps - 1) % cycle_steps;
        motor->current_pulses--;
    }

    /* === 同步角度 === */
    motor->current_angle = pulses_to_angle(motor->current_pulses,
                                           motor->step_mode);

    /* === 梯形加减速状态机 === */
    motor->phase_step_count++;
    uint32_t steps_taken = motor->phase_step_count;

    if (steps_taken < motor->accel_steps) {
        motor->trap_phase = TRAPEZOID_ACCEL;
    } else if (steps_taken < motor->accel_steps + motor->cruise_steps) {
        motor->trap_phase = TRAPEZOID_CRUISE;
    } else if (steps_taken < motor->total_steps) {
        motor->trap_phase = TRAPEZOID_DECEL;
    } else {
        motor->trap_phase       = TRAPEZOID_DONE;
        motor->base.state       = MOTOR_STATE_STOPPED;
        motor->phase_step_count = 0;
    }

    if (motor->trap_phase != TRAPEZOID_DONE) {
        stepper_update_trapezoid(motor);
    }

    return 0;
}

static int StepperMotor_cleanup(void *self)
{
    StepperMotor_t *motor = (StepperMotor_t *)self;

    for (uint8_t i = 0; i < STEPPER_PHASE_COUNT; i++) {
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    motor->base.state       = MOTOR_STATE_STOPPED;
    motor->phase_step_count = 0;
    motor->trap_phase       = TRAPEZOID_DONE;

    return 0;
}

static void StepperMotor_reset(void *self)
{
    StepperMotor_t *motor = (StepperMotor_t *)self;

    for (uint8_t i = 0; i < STEPPER_PHASE_COUNT; i++) {
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    motor->base.state       = MOTOR_STATE_STOPPED;
    motor->step_index       = 0;
    motor->target_angle     = 0;
    motor->current_angle    = 0;
    motor->target_pulses    = 0;
    motor->current_pulses   = 0;
    motor->phase_step_count = 0;
    motor->trap_phase       = TRAPEZOID_DONE;
    motor->direction        = 0;
}

/* ==================== 子类虚函数表 ==================== */

static const MotorVTable stepper_vtable = {
    .init    = StepperMotor_init,
    .run     = StepperMotor_run,
    .cleanup = StepperMotor_cleanup,
    .reset   = StepperMotor_reset,
};

/* ==================== 公有接口实现 ==================== */

void StepperMotor_Constructor(StepperMotor_t *self, const char *name,
                              const StepperPinConfig *pins)
{
    if (self == NULL) {
        return;
    }

    Motor_Constructor(&self->base, name);
    self->pins = pins;

    self->step_mode         = STEP_MODE_HALF_8;
    self->step_index        = 0;
    self->target_angle      = 0;
    self->current_angle     = 0;
    self->target_pulses     = 0;
    self->current_pulses    = 0;
    self->speed_grade       = 5;
    self->step_interval_us  = speed_interval_table[4];  /* grade 5: 800us */
    self->last_step_tick    = 0;
    self->accel_steps       = 0;
    self->cruise_steps      = 0;
    self->decel_steps       = 0;
    self->total_steps       = 0;
    self->phase_step_count  = 0;
    self->trap_phase        = TRAPEZOID_DONE;
    self->direction         = 0;

    self->base.vtable = &stepper_vtable;
}

void StepperMotor_SetAngle(StepperMotor_t *self, int32_t angle)
{
    if (self == NULL) {
        return;
    }

    if (self->base.state == MOTOR_STATE_RUNNING) {
        StepperMotor_EmergencyStop(self);
    }

    self->target_angle  = angle;
    self->target_pulses = angle_to_pulses(angle, self->step_mode);

    int32_t delta = self->target_pulses - self->current_pulses;
    uint32_t total;

    if (delta > 0) {
        self->direction = 0;
        total = (uint32_t)delta;
    } else if (delta < 0) {
        self->direction = 1;
        total = (uint32_t)(-delta);
    } else {
        self->base.state = MOTOR_STATE_STOPPED;
        return;
    }

    if (total == 0) {
        self->base.state = MOTOR_STATE_STOPPED;
        return;
    }

    stepper_calc_trapezoid(self, total);
    stepper_start_trapezoid(self);
}

void StepperMotor_ResetAngle(StepperMotor_t *self)
{
    if (self == NULL) {
        return;
    }

    StepperMotor_EmergencyStop(self);
    self->target_angle   = 0;
    self->current_angle  = 0;
    self->target_pulses  = 0;
    self->current_pulses = 0;
}

void StepperMotor_SetMode(StepperMotor_t *self, StepperStepMode mode)
{
    if (self == NULL) {
        return;
    }
    if (mode > STEP_MODE_WAVE_4) {
        return;
    }

    if (self->base.state == MOTOR_STATE_RUNNING) {
        StepperMotor_EmergencyStop(self);
    }

    self->step_mode  = mode;
    self->step_index = 0;

    self->current_pulses = angle_to_pulses(self->current_angle, mode);
    self->target_pulses  = angle_to_pulses(self->target_angle, mode);
}

void StepperMotor_SetSpeed(StepperMotor_t *self, uint8_t grade)
{
    if (self == NULL) {
        return;
    }
    if (grade < 1)  grade = 1;
    if (grade > 10) grade = 10;

    self->speed_grade = grade;

    if (self->base.state == MOTOR_STATE_RUNNING) {
        stepper_update_trapezoid(self);
    }
}

int32_t StepperMotor_GetAngle(const StepperMotor_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->current_angle;
}

int32_t StepperMotor_GetPulses(const StepperMotor_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->current_pulses;
}

void StepperMotor_EmergencyStop(StepperMotor_t *self)
{
    if (self == NULL) {
        return;
    }

    for (uint8_t i = 0; i < STEPPER_PHASE_COUNT; i++) {
        HAL_GPIO_WritePin(self->pins[i].port, self->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    self->base.state       = MOTOR_STATE_STOPPED;
    self->phase_step_count = 0;
    self->trap_phase       = TRAPEZOID_DONE;
}
