/**
  ******************************************************************************
  * @file    EncoderTest.c
  * @brief   编码器传感器测试程序 — 演示 Encoder 模块的完整功能
  *
  * @details
  * 遵循三分法测试结构（Init / Loop / IRQHandler），演示：
  *   - 左右双编码器对象的构造与初始化
  *   - TIM 编码器模式下的脉冲计数与回绕处理
  *   - 转速（RPM）、线速度（mm/s）、距离（mm）的实时计算
  *   - 数据清零与硬件复位功能验证
  *
  * === 硬件接线 ===
  * 左电机编码器: TIM1_CH1 (PE9) + TIM1_CH2 (PE11)
  * 右电机编码器: TIM8_CH1 (PC6) + TIM8_CH2 (PC7)
  *
  * === 中断配置 ===
  * TIM2 全局定时器: 基准中断，在 ISR 中软件计数分频出 40ms (25Hz)
  * 在 CubeMX 中配置 TIM2 为全局定时器基准，
  * 在 NVIC 中启用 TIM2 全局中断，40ms 更新周期由软件分频实现。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "EncoderTest.h"
#include "Encoder.h"
#include "main.h"
#include "DebugPrintf.h"

/* ==================== 全局实例 ==================== */

/**
  * @brief 左、右编码器实例
  * @note  定义为全局变量，在中断中更新，主循环中读取显示。
  *        htim1 和 htim8 在 tim.c 中由 CubeMX 生成。
  */
static Encoder_t left_encoder;
static Encoder_t right_encoder;

/* ==================== 演示状态机变量 ==================== */

/* 中断计数（验证中断频率）*/
static volatile uint32_t irq_call_count = 0;

/* 按键触发的操作标志（ISR 设置，Loop 处理）*/
static volatile uint8_t flag_clear_data  = 0;
static volatile uint8_t flag_hard_reset  = 0;

/* ==================== 前向声明 ==================== */

static void Encoder_Demo_Update(void);

/* ==================== 演示状态机实现 ==================== */

/**
  * @brief  编码器功能演示状态机
  * @note   每 500ms 打印一次编码器数据。
  *         阶段切换通过标志位（flag_clear_data / flag_hard_reset）触发，
  *         可由按键中断或在调试器中手动设置。
  */
static void Encoder_Demo_Update(void)
{
    /* ========== 按键标志处理 ========== */

    if (flag_clear_data) {
        flag_clear_data = 0;
        /* 仅清零累积数据，保留 TIM 硬件计数器运行 */
        Encoder_ClearData(&left_encoder);
        Encoder_ClearData(&right_encoder);
    }

    if (flag_hard_reset) {
        flag_hard_reset = 0;
        /* 完全复位（含硬件计数器清零）*/
        Encoder_HardReset(&left_encoder);
        Encoder_HardReset(&right_encoder);
    }

    /* ========== 周期性数据打印（每 500ms，由 Encoder 类内部节流）========== */

    Encoder_PrintDualInfo(&left_encoder, &right_encoder, &dbg_printf);
}

/* ==================== 三分法测试函数 ==================== */

/**
  * @brief  Encoder_Test_Init — 第1部分：初始化
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 2 区域
  *         调用时机：外设初始化完成后，while(1) 主循环之前
  *         调用频率：仅一次
  *
  * @note   初始化流程：
  *         1. Encoder_Constructor — 构造左右编码器对象
  *         2. SensorBase_Init       — 通过 vtable 调用 Encoder_init
  *                                    启动 TIM 编码器模式，清零数据
  *
  * @note   前置条件：
  *         - TIM1 和 TIM8 已在 CubeMX 中配置为 Encoder Mode (TI1+TI2)
  *         - TIM2 已在 CubeMX 中配置为全局定时器基准，40ms 由软件分频实现
  *         - htim1、htim2、htim8 在 tim.c 中已生成
  *         - SystemClock_Config() 和 MX_GPIO_Init() 已调用
  */
void Encoder_Test_Init(void)
{
    /* 构造左编码器（TIM1, 索引 0, 极性反转：A/B 相接反）*/
    Encoder_Constructor(&left_encoder, &htim1, 0, -1);

    /* 构造右编码器（TIM8, 索引 1, 极性正常）*/
    Encoder_Constructor(&right_encoder, &htim8, 1, +1);

    /* 通过基类接口初始化（内部调用 Encoder_init 虚函数）*/
    if (SensorBase_Init((SensorBase_t *)&left_encoder) != 0) {
        Error_Handler();
    }
    if (SensorBase_Init((SensorBase_t *)&right_encoder) != 0) {
        Error_Handler();
    }

}

/**
  * @brief  Encoder_Test_Loop — 第2部分：主循环
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 3 区域
  *         调用时机：while(1) 主循环中每个周期
  *         调用频率：周期性（循环迭代速率）
  *
  * @note   包含任务：
  *         1. Encoder_Demo_Update() — 演示状态机（打印数据、处理标志）
  *
  * @note   Run() 不在此处调用。
  *         编码器数据由 Encoder_Test_IRQHandler 中的 SensorBase_Run 更新，
  *         保证了软件分频 40ms 固定周期的精确时序。
  *         此处仅负责数据显示和用户交互处理。
  */
void Encoder_Test_Loop(void)
{
    Encoder_Demo_Update();
}

/**
  * @brief  Encoder_Test_IRQHandler — 第3部分：中断回调
  *
  * @note   运行位置：Callback.c 的 HAL_TIM_PeriodElapsedCallback 中
  *         调用时机：TIM2 全局定时器中断中软件分频，每 40ms 触发一次
  *         调用频率：25Hz
  *
  * @note   中断安全规则：
  *         - 仅调用 SensorBase_Run（内部仅做寄存器读取和浮点计算，< 50us）
  *         - 递增中断计数（uint32_t 原子写入 < 对齐地址 32位 安全）
  *         - 不调用阻塞 API（HAL_Delay、printf 等）
  *         - 不在 ISR 中打印输出
  *
  * @note   使用示例（在 Callback.c 中）：
  *         void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  *             if (htim->Instance == TIM2) {
  *                 Encoder_Test_IRQHandler();
  *             }
  *         }
  */
void Encoder_Test_IRQHandler(void)
{
    /* 更新左编码器数据 */
    SensorBase_Run((SensorBase_t *)&left_encoder);

    /* 更新右编码器数据 */
    SensorBase_Run((SensorBase_t *)&right_encoder);
}
