#ifndef __KEY_TEST_H__
#define __KEY_TEST_H__

/* ==================== 三分法测试接口 ==================== */

/**
  * @brief  Key 测试 — 第1部分：初始化
  * @note   运行于 main() 的 USER CODE BEGIN 2 区域，
  *         在外设初始化完成后、主循环前调用一次。
  *         负责：构造 4 个 Key 对象、模块初始化、一次性配置。
  */
void Key_Test_Init(void);

/**
  * @brief  Key 测试 — 第2部分：主循环
  * @note   运行于 main() 的 USER CODE BEGIN 3 区域，
  *         在 while(1) 循环中周期性调用。
  *         负责：查询按键事件并执行对应动作（LED/蜂鸣器交互）。
  */
void Key_Test_Loop(void);

/**
  * @brief  Key 测试 — 第3部分：中断回调
  * @note   运行于 Callback.c 的中断回调函数中，
  *         由定时器中断触发（1ms 周期）。
  *         负责：为每个 Key 对象调用 Key_Tick() 驱动状态机。
  */
void Key_Test_IRQHandler(void);

#endif /* __KEY_TEST_H__ */
