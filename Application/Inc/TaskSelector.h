/**
  ******************************************************************************
  * @file    TaskSelector.h
  * @brief   按键选择任务模块 — KEY1-4 分别启动 Task1-4
  *
  * @details
  * 使用 Key 模块的单击检测，在系统启动后等待用户按下按键选择任务:
  *   - KEY1 (PC1): 启动 Task1 — 正方形边框逆时针循迹一圈
  *   - KEY2 (PC3): 启动 Task2 — 正方形边框顺时针循迹一圈 (含180°掉头)
  *   - KEY3 (PA0): 启动 Task3 — 负重场景循迹 (含减速带/加速带)
  *   - KEY4 (PA1): 启动 Task4 — 180°CW掉头→巡线→第1边沿90°转→400pwm/500ms
  *
  * 状态机:
  *   IDLE → (按键单击) → RUNNING → (任务完成) → IDLE
  *
  * Key_Tick 由 TaskSelector_IRQHandler() 在 TIM2 1ms ISR 中驱动,
  * Key_Check 由 TaskSelector_Loop() 在主循环中轮询。
  ******************************************************************************
  */

#ifndef __TASK_SELECTOR_H__
#define __TASK_SELECTOR_H__

void TaskSelector_Init(void);
void TaskSelector_IRQHandler(void);
void TaskSelector_Loop(void);

#endif /* __TASK_SELECTOR_H__ */
