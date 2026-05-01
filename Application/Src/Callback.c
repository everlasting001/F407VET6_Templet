#include "Callback.h"
#include "tim.h"

/* 定时分频标志定义 */
volatile uint8_t Flag_1ms    = 0;
volatile uint8_t Flag_10ms   = 0;
volatile uint8_t Flag_100ms  = 0;
volatile uint8_t Flag_500ms  = 0;
volatile uint8_t Flag_1000ms = 0;

/**
  * @brief  初始化定时回调模块：启动 TIM2 更新中断（1ms）
  */
void Callback_Init(void)
{
    HAL_TIM_Base_Start_IT(&htim2);
}

/**
  * @brief  TIM 周期中断回调（1ms 基时 + 软件分频）
  *
  * TIM2 配置 (CubeMX):
  *   APB1 Timer Clock = 84MHz, Prescaler = 84-1, Period = 1000-1 → 1ms
  *
  * 软件分频输出:
  *   Flag_1ms    → 每 1ms  置位
  *   Flag_10ms   → 每 10ms 置位
  *   Flag_100ms  → 每 100ms 置位
  *   Flag_500ms  → 每 500ms 置位 + LED(PC0) 翻转测试
  *   Flag_1000ms → 每 1000ms 置位 + Buzzer(PC2) 翻转测试
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim2) {
        static uint16_t Cnt_10ms   = 0;
        static uint16_t Cnt_100ms  = 0;
        static uint16_t Cnt_500ms  = 0;
        static uint16_t Cnt_1000ms = 0;

        /* 1ms 基准标志 */
        Flag_1ms = 1;
        Key_Test_IRQHandler();
        /* 软件分频计数器递增 */
        Cnt_10ms++;
        Cnt_100ms++;
        Cnt_500ms++;
        Cnt_1000ms++;

        /* 10ms 分频 */
        if (Cnt_10ms >= 10) {
            Flag_10ms = 1;
            Cnt_10ms = 0;
        }

        /* 100ms 分频 */
        if (Cnt_100ms >= 100) {
            Flag_100ms = 1;
            Cnt_100ms = 0;
        }

        /* 500ms 分频 — LED 翻转测试 */
        if (Cnt_500ms >= 500) {
            Flag_500ms = 1;
            LED_Test_IRQHandler();
            Cnt_500ms = 0;
        }

        /* 1000ms 分频*/
        if (Cnt_1000ms >= 1000) {
            Flag_1000ms = 1;
            Cnt_1000ms = 0;
        }
    }
}
