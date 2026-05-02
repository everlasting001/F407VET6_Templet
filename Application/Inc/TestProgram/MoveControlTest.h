/**
  ******************************************************************************
  * @file    MoveControlTest.h
  * @brief   运动控制测试 — 双轮差速 1000mm 定位置 PID 控制 Demo
  *
  * @details
  * 三分法测试结构：
  *   - MoveControl_Test_Init()      → 构造对象、初始化、设置目标
  *   - MoveControl_Test_Loop()      → 主循环中调用位置控制
  *   - MoveControl_Test_IRQHandler()→ 中断中更新编码器数据
  ******************************************************************************
  */

#ifndef __MOVE_CONTROL_TEST_H__
#define __MOVE_CONTROL_TEST_H__

void MoveControl_Test_Init(void);
void MoveControl_Test_Loop(void);
void MoveControl_Test_IRQHandler(void);

#endif /* __MOVE_CONTROL_TEST_H__ */
