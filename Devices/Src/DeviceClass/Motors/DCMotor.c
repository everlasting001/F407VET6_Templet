/**
  ******************************************************************************
  * @file    DCMotor.c
  * @brief   DC 有刷电机子类实现 — TB6612FNG 驱动控制
  *
  * @details
  * 提供 DCMotor_t 的完整实现，包括:
  *   - 虚函数表 (init / run / cleanup / reset)
  *   - PWM 通过 __HAL_TIM_SET_COMPARE() 设置 (ARR=2099, 20kHz)
  *   - TB6612 方向控制通过 AIN1/AIN2 GPIO
  *   - 可配置方向极性 (CCW/CW 互换)
  *
  * === TIM3 时钟计算 ===
  *
  * HCLK   = 168 MHz (STM32F407 最大)
  * APB1   =  42 MHz (HCLK / 4)
  * TIM3   =  84 MHz (APB1 × 2, 因 APB1 prescaler ≠ 1)
  * PSC    =   1 (÷2)
  * CNT    =  42 MHz (84 MHz / 2)
  * ARR    = 2099
  * f_PWM  =  42 MHz / 2100 = 20 kHz
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "DCMotor.h"
#include "stm32f4xx_hal.h"

/* ==================== 虚函数实现 ==================== */

static int DCMotor_init(void *self)
{
    DCMotor_t *motor = (DCMotor_t *)self;

    if (motor->pins == NULL || motor->htim == NULL) {
        return -1;
    }

    /* 方向引脚初始电平拉低 */
    for (uint8_t i = 0; i < DCMOTOR_PIN_COUNT; i++) {
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    /* 启动 PWM 通道, 初始占空比 0 */
    HAL_TIM_PWM_Start(motor->htim, motor->tim_channel);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->tim_channel, 0);

    motor->compare_value = 0;
    motor->direction     = DCMOTOR_DIR_FORWARD;
    motor->base.state    = MOTOR_STATE_STOPPED;

    return 0;
}

static int DCMotor_run(void *self)
{
    DCMotor_t *motor = (DCMotor_t *)self;
    (void)motor;
    return 0;
}

static int DCMotor_cleanup(void *self)
{
    DCMotor_t *motor = (DCMotor_t *)self;

    __HAL_TIM_SET_COMPARE(motor->htim, motor->tim_channel, 0);
    HAL_TIM_PWM_Stop(motor->htim, motor->tim_channel);

    for (uint8_t i = 0; i < DCMOTOR_PIN_COUNT; i++) {
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    motor->base.state    = MOTOR_STATE_STOPPED;
    motor->compare_value = 0;

    return 0;
}

static void DCMotor_reset(void *self)
{
    DCMotor_t *motor = (DCMotor_t *)self;

    __HAL_TIM_SET_COMPARE(motor->htim, motor->tim_channel, 0);
    HAL_TIM_PWM_Stop(motor->htim, motor->tim_channel);

    for (uint8_t i = 0; i < DCMOTOR_PIN_COUNT; i++) {
        HAL_GPIO_WritePin(motor->pins[i].port, motor->pins[i].pin,
                          GPIO_PIN_RESET);
    }

    motor->base.state    = MOTOR_STATE_STOPPED;
    motor->compare_value = 0;
    motor->direction     = DCMOTOR_DIR_FORWARD;
}

/* ==================== 子类虚函数表 ==================== */

static const MotorVTable dcmotor_vtable = {
    .init    = DCMotor_init,
    .run     = DCMotor_run,
    .cleanup = DCMotor_cleanup,
    .reset   = DCMotor_reset,
};

/* ==================== 公有接口实现 ==================== */

void DCMotor_Constructor(DCMotor_t *self, const char *name,
                         const DCMotorPinConfig *pins,
                         TIM_HandleTypeDef *htim, uint32_t tim_channel)
{
    if (self == NULL) {
        return;
    }

    Motor_Constructor(&self->base, name);
    self->pins          = pins;
    self->htim          = htim;
    self->tim_channel   = tim_channel;
    self->polarity      = 1;
    self->compare_value = 0;
    self->direction     = DCMOTOR_DIR_FORWARD;

    self->base.vtable = &dcmotor_vtable;
}

void DCMotor_SetSpeed(DCMotor_t *self, int16_t speed)
{
    if (self == NULL) {
        return;
    }

    if (speed == 0) {
        DCMotor_Stop(self);
        return;
    }

    uint16_t abs_speed;
    DCMotorDir dir;

    if (speed > 0) {
        abs_speed = (uint16_t)speed;
        dir       = DCMOTOR_DIR_FORWARD;
    } else {
        abs_speed = (uint16_t)(-speed);
        dir       = DCMOTOR_DIR_BACKWARD;
    }

    if (abs_speed > DCMOTOR_ARR_MAX) {
        abs_speed = DCMOTOR_ARR_MAX;
    }

    self->compare_value = abs_speed;
    self->direction     = dir;

    /* TB6612 方向真值表 (polarity>0):
       FORWARD(CCW):  AIN1=1, AIN2=0 → OUT1=L, OUT2=H
       BACKWARD(CW):  AIN1=0, AIN2=1 → OUT1=H, OUT2=L
       polarity<0 时翻转 */
    GPIO_PinState ain1_state, ain2_state;

    if (dir == DCMOTOR_DIR_FORWARD) {
        ain1_state = (self->polarity > 0) ? GPIO_PIN_SET   : GPIO_PIN_RESET;
        ain2_state = (self->polarity > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    } else {
        ain1_state = (self->polarity > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
        ain2_state = (self->polarity > 0) ? GPIO_PIN_SET   : GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(self->pins[DCMOTOR_PIN_AIN1].port,
                      self->pins[DCMOTOR_PIN_AIN1].pin, ain1_state);
    HAL_GPIO_WritePin(self->pins[DCMOTOR_PIN_AIN2].port,
                      self->pins[DCMOTOR_PIN_AIN2].pin, ain2_state);

    __HAL_TIM_SET_COMPARE(self->htim, self->tim_channel, abs_speed);
    self->base.state = MOTOR_STATE_RUNNING;
}

void DCMotor_Stop(DCMotor_t *self)
{
    if (self == NULL) {
        return;
    }

    /* PWM 比较值清零 */
    __HAL_TIM_SET_COMPARE(self->htim, self->tim_channel, 0);

    /* 方向引脚均拉低 (TB6612 滑行停止: OUT1/OUT2 高阻) */
    HAL_GPIO_WritePin(self->pins[DCMOTOR_PIN_AIN1].port,
                      self->pins[DCMOTOR_PIN_AIN1].pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(self->pins[DCMOTOR_PIN_AIN2].port,
                      self->pins[DCMOTOR_PIN_AIN2].pin, GPIO_PIN_RESET);

    self->compare_value = 0;
    self->base.state    = MOTOR_STATE_STOPPED;
}

void DCMotor_SetDirection(DCMotor_t *self, DCMotorDir dir)
{
    if (self == NULL) {
        return;
    }

    if (self->direction == dir) {
        return;
    }

    self->direction = dir;

    /* 更新方向 GPIO, PWM 比较值保持不变
       TB6612: FORWARD(CCW) → AIN1=1,AIN2=0; BACKWARD(CW) → AIN1=0,AIN2=1 */
    GPIO_PinState ain1_state, ain2_state;

    if (dir == DCMOTOR_DIR_FORWARD) {
        ain1_state = (self->polarity > 0) ? GPIO_PIN_SET   : GPIO_PIN_RESET;
        ain2_state = (self->polarity > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    } else {
        ain1_state = (self->polarity > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET;
        ain2_state = (self->polarity > 0) ? GPIO_PIN_SET   : GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(self->pins[DCMOTOR_PIN_AIN1].port,
                      self->pins[DCMOTOR_PIN_AIN1].pin, ain1_state);
    HAL_GPIO_WritePin(self->pins[DCMOTOR_PIN_AIN2].port,
                      self->pins[DCMOTOR_PIN_AIN2].pin, ain2_state);

    if (self->compare_value > 0) {
        self->base.state = MOTOR_STATE_RUNNING;
    }
}

uint16_t DCMotor_GetSpeed(const DCMotor_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->compare_value;
}

DCMotorDir DCMotor_GetDirection(const DCMotor_t *self)
{
    if (self == NULL) {
        return DCMOTOR_DIR_FORWARD;
    }
    return self->direction;
}
