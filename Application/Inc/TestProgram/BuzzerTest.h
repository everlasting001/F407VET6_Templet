#ifndef __BUZZER_TEST_H__
#define __BUZZER_TEST_H__

/* ==================== 三分法测试接口 ==================== */

/**
  * @brief  蜂鸣器测试 — 第1部分：初始化
  * @note   运行于 main() 的 USER CODE BEGIN 2 区域，
  *         在外设初始化完成后、主循环前调用一次。
  *         负责：构造对象、模块初始化、一次性配置。
  */
void BuzzerTest_Init(void);

/**
  * @brief  蜂鸣器测试 — 第2部分：主循环
  * @note   运行于 main() 的 USER CODE BEGIN 3 区域，
  *         在 while(1) 循环中周期性调用。
  *         负责：周期性刷新（ModuleBase_Run）、演示状态机更新。
  */
void BuzzerTest_Loop(void);

/**
  * @brief  蜂鸣器测试 — 第3部分：中断回调
  * @note   运行于 Callback.c 的中断回调函数中（如 HAL_TIM_PeriodElapsedCallback），
  *         由硬件中断触发，需保持快速执行。
  *         负责：中断触发的蜂鸣器事件处理。
  */
void BuzzerTest_IRQHandler(void);

#endif /* __BUZZER_TEST_H__ */
