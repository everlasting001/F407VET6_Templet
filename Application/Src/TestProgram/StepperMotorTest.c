/**
  ******************************************************************************
  * @file    StepperMotorTest.c
  * @brief   步进电机测试程序 — 三分法结构 (Init / Loop / IRQHandler)
  *
  * @details
  * 测试场景：
  *   使用 PA8(IN1), PA9(IN2), PA10(IN3), PA11(IN4) 连接 ULN2003 驱动板，
  *   驱动 28BYJ-48 步进电机。
  *
  *   初始化完成后，每 5 秒切换一次目标角度：
  *     0° → 90° → 180° → -90° → 0° → ...
  *   演示梯形加减速、多模式切换和角度复位。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "StepperMotorTest.h"
#include "StepperMotor.h"
#include "dwt_delay.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* ==================== 引脚配置 ==================== */

/**
  * @brief 28BYJ-48 四相引脚定义 (ULN2003 IN1~IN4)
  * @note  可根据实际接线修改
  */
static const StepperPinConfig stepper_pins[4] = {
    {V_IN1_GPIO_Port, V_IN1_Pin},   /* IN1 */
    {V_IN2_GPIO_Port, V_IN2_Pin},   /* IN2 */
    {V_IN3_GPIO_Port, V_IN3_Pin},  /* IN3 */
    {V_IN4_GPIO_Port, V_IN4_Pin},  /* IN4 */
};

/* ==================== 测试对象 ==================== */

static StepperMotor_t g_test_motor;

/* ==================== 测试状态 ==================== */

static uint32_t      s_last_switch_tick = 0;
static uint8_t       s_angle_index      = 0;

/* 测试角度序列 (定点小数, 0.01° 单位) */
static const int32_t s_test_angles[] = {
    9000,   /*  90.00° */
    6000,   /* 60.00° */
    -9000,   /* -90.00° */
    0,   /*   0.00° (复位) */
    -6000,   /* -60.00° */
    4500,   /*  45.00° */
};

/* ==================== 三分法实现 ==================== */

void StepperMotor_Test_Init(void)
{
    /* 0. 初始化 DWT 周期计数器 (用于步进电机微秒级时序) */
    DWT_Init();

    /* 1. 构造步进电机对象 */
    StepperMotor_Constructor(&g_test_motor, "StepMotor_Test", stepper_pins);

    /* 2. 通过基类接口初始化 */
    Motor_Init(&g_test_motor.base);

    /* 3. 初始状态：角度归零，转速等级 5 (默认) */
    StepperMotor_ResetAngle(&g_test_motor);
    StepperMotor_SetSpeed(&g_test_motor, 5);

    s_last_switch_tick = HAL_GetTick();
    s_angle_index      = 0;

    // StepperMotor_SetAngle(&g_test_motor, 6000);
}

void StepperMotor_Test_Loop(void)
{
    /* 1. 周期性调用电机运行逻辑 (DWT 非阻塞步进) */
    Motor_Run(&g_test_motor.base);

    /* 2. 每 5 秒切换一次目标角度 (演示) */
    uint32_t now = HAL_GetTick();
    if (now - s_last_switch_tick >= 1000) {
        s_last_switch_tick = now;

        int32_t target = s_test_angles[s_angle_index];
        s_angle_index = (s_angle_index + 1)
                        % (sizeof(s_test_angles) / sizeof(s_test_angles[0]));

        StepperMotor_SetAngle(&g_test_motor, target);
    }
}

void StepperMotor_Test_IRQHandler(void)
{
    /* DWT 非阻塞时序不需要 ISR 干预，此函数保留为空 */
    (void)0;
}
