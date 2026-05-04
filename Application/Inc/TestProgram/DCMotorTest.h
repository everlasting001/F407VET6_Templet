/**
  ******************************************************************************
  * @file    DCMotorTest.h
  * @brief   DC 有刷电机测试程序接口 — 三分法结构 (Init / Loop / IRQHandler)
  *
  * @details
  * 测试场景：
  *   同时对 Left_DCMotor 和 Right_DCMotor 进行功能测试，
  *   演示速度渐变、方向切换、独立启停和极性配置。
  *
  ******************************************************************************
  */

#ifndef __DCMOTOR_TEST_H__
#define __DCMOTOR_TEST_H__

/* ==================== 三分法测试接口 ==================== */

/**
  * @brief  DC 电机测试 — 第1部分：初始化
  * @note   运行于 main() 的 USER CODE BEGIN 2 区域，
  *         在外设初始化完成后、主循环前调用一次。
  *         负责：构造两个 DCMotor 对象、绑定引脚、PWM 初始化。
  */
void DCMotor_Test_Init(void);

/**
  * @brief  DC 电机测试 — 第2部分：主循环
  * @note   运行于 main() 的 USER CODE BEGIN 3 区域，
  *         在 while(1) 循环中周期性调用。
  *         负责：演示状态机 — 正转加速、反转、停止、换向。
  */
void DCMotor_Test_Loop(void);

/**
  * @brief  DC 电机测试 — 第3部分：中断回调
  * @note   运行于 Callback.c 的中断回调函数中。
  *         当前 DC 电机控制无需 ISR 干预，保留为兼容接口。
  */
void DCMotor_Test_IRQHandler(void);

#endif /* __DCMOTOR_TEST_H__ */
