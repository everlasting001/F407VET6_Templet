#ifndef __ENCODER_TEST_H__
#define __ENCODER_TEST_H__

/* ==================== 三分法测试接口 ==================== */

/**
  * @brief  编码器测试 — 第1部分：初始化
  * @note   运行于 main() 的 USER CODE BEGIN 2 区域，
  *         在外设初始化完成后、主循环前调用一次。
  *         负责：构造左右编码器对象、模块初始化、一次性配置。
  */
void Encoder_Test_Init(void);

/**
  * @brief  编码器测试 — 第2部分：主循环
  * @note   运行于 main() 的 USER CODE BEGIN 3 区域，
  *         在 while(1) 循环中周期性调用。
  *         负责：周期性打印编码器数据（RPM、速度、距离等）。
  */
void Encoder_Test_Loop(void);

/**
  * @brief  编码器测试 — 第3部分：中断回调
  * @note   运行于 Callback.c 的 HAL_TIM_PeriodElapsedCallback 中，
  *         由 TIM2 全局定时器中断触发（周期 40ms / 25Hz）。
  *         负责：调用 SensorBase_Run 更新编码器读数。
  *         保持快速执行，仅调用 Run 和设置标志位。
  *
  * @note   使用示例（在 Callback.c 中）：
  *         void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  *             if (htim->Instance == TIM2) {
  *                 Encoder_Test_IRQHandler();
  *             }
  *         }
  */
void Encoder_Test_IRQHandler(void);

#endif /* __ENCODER_TEST_H__ */
