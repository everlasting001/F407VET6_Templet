/**
  ******************************************************************************
  * @file    MoveControlTest.c
  * @brief   运动控制测试 — 双轮差速 1000mm 定位置 PID 控制 Demo
  *
  * @details
  * 三分法结构：
  *   Part 1 — Init:      构造 DCMotor + Encoder + MoveControl, 设目标 1000mm
  *   Part 2 — Loop:      每 40ms 调用 MoveControl_RunPositionMode(),
  *                        到达目标后通过 DebugPrintf 打印完成信息
  *   Part 3 — IRQHandler: 40ms ISR 中更新左右编码器数据 (SensorBase_Run)
  *
  * === 硬件接线 ===
  *
  * 左电机: AIN1→PD0, AIN2→PD1, PWMA→PA6(TIM3_CH1), 编码器→TIM1(PE9/PE11)
  * 右电机: BIN1→PD2, BIN2→PD3, PWMB→PA7(TIM3_CH2), 编码器→TIM8(PC6/PC7)
  *
  * === 控制链路 ===
  *
  * IRQHandler (40ms):    Encoder Run → 更新 pulse_diff/rpm/mmps/distance_mm
  * Loop    (40ms flag):  MoveControl_RunPositionMode → 读编码器 → BangBang+PD → 写PWM
  *
  ******************************************************************************
  */

#include "MoveControlTest.h"
#include "DCMotor.h"
#include "Encoder.h"
#include "MoveControl.h"
#include "main.h"
#include "DebugPrintf.h"
#include "Callback.h"

/* ==================== 引脚配置 ==================== */

static const DCMotorPinConfig left_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {AIN1_GPIO_Port, AIN1_Pin},
    [DCMOTOR_PIN_AIN2] = {AIN2_GPIO_Port, AIN2_Pin},
};

static const DCMotorPinConfig right_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {BIN1_GPIO_Port, BIN1_Pin},
    [DCMOTOR_PIN_AIN2] = {BIN2_GPIO_Port, BIN2_Pin},
};

/* ==================== 全局实例 ==================== */

static DCMotor_t     g_left_motor;
static DCMotor_t     g_right_motor;
static Encoder_t     g_left_encoder;
static Encoder_t     g_right_encoder;
static MoveControl_t g_move_ctrl;

/* ==================== 测试状态 ==================== */

static uint32_t s_last_print_tick = 0;
static uint8_t  s_printed_done    = 0;

/* ==================== 三分法实现 ==================== */

/**
  * @brief  MoveControl_Test_Init — Part 1：初始化
  * @note   运行于 main() 的 USER CODE BEGIN 2，外设初始化后、while 前。
  */
void MoveControl_Test_Init(void)
{
    /* 1. 构造左电机: TIM3_CH1 (PA6) */
    DCMotor_Constructor(&g_left_motor, "Left_DCMotor", left_pins,
                        &htim3, TIM_CHANNEL_1);
    if (Motor_Init(&g_left_motor.base) != 0) {
        Error_Handler();
    }

    /* 2. 构造右电机: TIM3_CH2 (PA7) */
    DCMotor_Constructor(&g_right_motor, "Right_DCMotor", right_pins,
                        &htim3, TIM_CHANNEL_2);
    if (Motor_Init(&g_right_motor.base) != 0) {
        Error_Handler();
    }

    /* 3. 构造左编码器: TIM1, 索引 0, 极性反转 */
    Encoder_Constructor(&g_left_encoder, &htim1, 0, -1);
    if (SensorBase_Init((SensorBase_t *)&g_left_encoder) != 0) {
        Error_Handler();
    }

    /* 4. 构造右编码器: TIM8, 索引 1, 极性正常 */
    Encoder_Constructor(&g_right_encoder, &htim8, 1, +1);
    if (SensorBase_Init((SensorBase_t *)&g_right_encoder) != 0) {
        Error_Handler();
    }

    /* 5. 初始化运动控制系统 */
    MoveControl_Init(&g_move_ctrl,
                     &g_left_motor, &g_right_motor,
                     &g_left_encoder, &g_right_encoder);

    /* 6. 设置目标 1000mm */
    MoveControl_SetTarget(&g_move_ctrl, 1000.0f);

    s_last_print_tick = 0;
    s_printed_done    = 0;
}

/**
  * @brief  MoveControl_Test_Loop — Part 2：主循环
  * @note   运行于 main() 的 while(1) 主循环中。
  *         由 Flag_40ms 节流，与编码器更新周期一致。
  *         检查完成状态，到达目标后打印信息。
  */
void MoveControl_Test_Loop(void)
{
    /* 40ms 周期节流：由 IRQHandler 中的 Flag_40ms 标记控制 */
    if (!Flag_40ms) {
        return;
    }
    Flag_40ms = 0;

    /* 运行位置控制 */
    uint8_t done = MoveControl_RunPositionMode(&g_move_ctrl);

    /* 每 50ms 打印一次状态 */
    uint32_t now = HAL_GetTick();
    if (now - s_last_print_tick >= 50) {
        s_last_print_tick = now;

        float avg_dist = MoveControl_GetAvgDistance(&g_move_ctrl);
        float error    = MoveControl_GetPositionError(&g_move_ctrl);
        float rpm_l    = Encoder_GetRPM(&g_left_encoder);
        float rpm_r    = Encoder_GetRPM(&g_right_encoder);

        DebugPrintf_Print(&dbg_printf,
            "Dist=%.1f/%.0f Err=%.1f L=%.0f R=%.0f [rpm]%s",
            (double)avg_dist,
            (double)g_move_ctrl.target_mm,
            (double)error,
            (double)rpm_l,
            (double)rpm_r,
            done ? " DONE\r\n" : "\r\n");
    }

    /* 到达目标后仅打印一次 */
    if (done && !s_printed_done) {
        s_printed_done = 1;
        uint32_t elapsed = HAL_GetTick() - g_move_ctrl.start_tick;
        DebugPrintf_Print(&dbg_printf,
            "=== 1000mm PID Demo Complete ===\r\n"
            "Time=%lums Dist=%.1fmm\r\n",
            (unsigned long)elapsed,
            (double)MoveControl_GetAvgDistance(&g_move_ctrl));
    }
}

/**
  * @brief  MoveControl_Test_IRQHandler — Part 3：中断回调
  * @note   运行于 Callback.c 的 HAL_TIM_PeriodElapsedCallback 中，
  *         由 TIM2 1ms ISR 软件分频到 40ms（25Hz）。
  *         仅更新编码器数据，不调用阻塞 API。
  *         控制计算由 Loop 中的 MoveControl_RunPositionMode 完成。
  */
void MoveControl_Test_IRQHandler(void)
{
    SensorBase_Run((SensorBase_t *)&g_left_encoder);
    SensorBase_Run((SensorBase_t *)&g_right_encoder);
}
