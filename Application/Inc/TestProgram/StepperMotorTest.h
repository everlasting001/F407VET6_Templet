#ifndef __STEPPER_MOTOR_TEST_H__
#define __STEPPER_MOTOR_TEST_H__

/* ==================== 三分法测试接口 ==================== */

/**
  * @brief  步进电机测试 — 第1部分：初始化
  * @note   运行于 main() 的 USER CODE BEGIN 2 区域，
  *         在外设初始化完成后、主循环前调用一次。
  *         负责：构造 StepperMotor 对象、绑定引脚、模块初始化。
  */
void StepperMotor_Test_Init(void);

/**
  * @brief  步进电机测试 — 第2部分：主循环
  * @note   运行于 main() 的 USER CODE BEGIN 3 区域，
  *         在 while(1) 循环中周期性调用。
  *         负责：调用 Motor_Run() 执行步进逻辑。
  */
void StepperMotor_Test_Loop(void);

/**
  * @brief  步进电机测试 — 第3部分：中断回调 (已保留为空)
  * @note   原来委托 StepperMotor_IRQHandler 管理 TIM2 1ms 步进时序。
  *         现在改用 DWT 非阻塞时序，不再需要 ISR 干预。
  *         保留此接口以兼容原有调用点。
  */
void StepperMotor_Test_IRQHandler(void);

#endif /* __STEPPER_MOTOR_TEST_H__ */
