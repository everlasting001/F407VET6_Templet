/**
  ******************************************************************************
  * @file    DCMotorTest.c
  * @brief   DC 有刷电机测试程序 — 三分法结构 (Init / Loop / IRQHandler)
  *
  * @details
  * 测试场景：
  *   同时对 Left_DCMotor 和 Right_DCMotor 进行功能测试。
  *   左电机 引脚: AIN1→PD0, AIN2→PD1, PWMA→PA6 (TIM3_CH1)
  *   右电机 引脚: BIN1→PD2, BIN2→PD3, PWMA→PA7 (TIM3_CH2)
  *
  *   每 5 秒切换一个演示阶段：
  *     阶段0: 两电机正转加速 (0→2099 斜坡)
  *     阶段1: 两电机反转 50% 占空比
  *     阶段2: 左正转/右反转 (差速转向)
  *     阶段3: 两电机停止
  *     阶段4: 两电机全速正转
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "DCMotorTest.h"
#include "DCMotor.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* ==================== 左电机引脚配置 ==================== */

static const DCMotorPinConfig left_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {AIN1_GPIO_Port, AIN1_Pin},   /* AIN1 → PD0 */
    [DCMOTOR_PIN_AIN2] = {AIN2_GPIO_Port, AIN2_Pin},   /* AIN2 → PD1 */
};

/* ==================== 右电机引脚配置 ==================== */

static const DCMotorPinConfig right_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {BIN1_GPIO_Port, BIN1_Pin},   /* BIN1 → PD2 */
    [DCMOTOR_PIN_AIN2] = {BIN2_GPIO_Port, BIN2_Pin},   /* BIN2 → PD3 */
};

/* ==================== 测试对象 ==================== */

static DCMotor_t g_left_motor;
static DCMotor_t g_right_motor;

/* ==================== 演示阶段枚举 ==================== */

typedef enum {
    DCMOTOR_DEMO_ACCEL,         /* 阶段0: 正转加速  */
    DCMOTOR_DEMO_REVERSE,       /* 阶段1: 反转中速  */
    DCMOTOR_DEMO_DIFF,          /* 阶段2: 差速转向  */
    DCMOTOR_DEMO_STOP,          /* 阶段3: 全部停止  */
    DCMOTOR_DEMO_FULL,          /* 阶段4: 全速正转  */
    DCMOTOR_DEMO_COUNT
} DCMotorDemoPhase;

static DCMotorDemoPhase s_demo_phase      = DCMOTOR_DEMO_ACCEL;
static uint32_t         s_phase_start_tick = 0;

/* ==================== 三分法实现 ==================== */

void DCMotor_Test_Init(void)
{
    /* 1. 构造左电机对象: TIM3_CH1 (PA6) */
    DCMotor_Constructor(&g_left_motor, "Left_DCMotor", left_pins,
                        &htim3, TIM_CHANNEL_1);

    /* 2. 通过基类接口初始化 (启动 PWM, 拉低方向引脚) */
    Motor_Init(&g_left_motor.base);

    /* 3. 构造右电机对象: TIM3_CH2 (PA7) */
    DCMotor_Constructor(&g_right_motor, "Right_DCMotor", right_pins,
                        &htim3, TIM_CHANNEL_2);

    /* 4. 通过基类接口初始化 */
    Motor_Init(&g_right_motor.base);

    s_phase_start_tick = HAL_GetTick();
    s_demo_phase       = DCMOTOR_DEMO_ACCEL;
}

void DCMotor_Test_Loop(void)
{
    /* 1. 周期性调用电机运行逻辑 (维护状态) */
    Motor_Run(&g_left_motor.base);
    Motor_Run(&g_right_motor.base);
    DCMotor_SetSpeed(&g_left_motor, 500);
    DCMotor_SetSpeed(&g_right_motor, 500);
    // /* 2. 演示状态机: 每 5 秒切换阶段 */
    // uint32_t now     = HAL_GetTick();
    // uint32_t elapsed = now - s_phase_start_tick;

    // if (elapsed >= 5000) {
    //     s_demo_phase = (DCMotorDemoPhase)(((int)s_demo_phase + 1)
    //                                       % DCMOTOR_DEMO_COUNT);
    //     s_phase_start_tick = now;
    //     elapsed = 0;
    // }

    // switch (s_demo_phase) {

    // /* ========== 阶段0: 正转加速 (0→2099 线性斜坡) ========== */
    // case DCMOTOR_DEMO_ACCEL: {
    //     uint16_t speed = (uint16_t)(elapsed * DCMOTOR_ARR_MAX / 5000);
    //     DCMotor_SetSpeed(&g_left_motor,  (int16_t)speed);
    //     DCMotor_SetSpeed(&g_right_motor, (int16_t)speed);
    //     break;
    // }

    // /* ========== 阶段1: 反转 ~50% 占空比 ========== */
    // case DCMOTOR_DEMO_REVERSE: {
    //     DCMotor_SetSpeed(&g_left_motor,  -1050);
    //     DCMotor_SetSpeed(&g_right_motor, -1050);
    //     break;
    // }

    // /* ========== 阶段2: 差速转向 (左正转, 右反转) ========== */
    // case DCMOTOR_DEMO_DIFF: {
    //     DCMotor_SetSpeed(&g_left_motor,   1050);
    //     DCMotor_SetSpeed(&g_right_motor, -1050);
    //     break;
    // }

    // /* ========== 阶段3: 全部停止 ========== */
    // case DCMOTOR_DEMO_STOP: {
    //     DCMotor_Stop(&g_left_motor);
    //     DCMotor_Stop(&g_right_motor);
    //     break;
    // }

    // /* ========== 阶段4: 全速正转 ========== */
    // case DCMOTOR_DEMO_FULL: {
    //     DCMotor_SetSpeed(&g_left_motor,  (int16_t)DCMOTOR_ARR_MAX);
    //     DCMotor_SetSpeed(&g_right_motor, (int16_t)DCMOTOR_ARR_MAX);
    //     break;
    // }

    // default:
    //     break;
    // }
}

void DCMotor_Test_IRQHandler(void)
{
    /* DC 电机 PWM 直接由硬件 TIM3 产生, 无需 ISR 干预 */
}
