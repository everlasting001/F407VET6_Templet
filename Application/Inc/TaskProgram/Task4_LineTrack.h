/**
  ******************************************************************************
  * @file    Task4_LineTrack.h
  * @brief   Task4 — 初始180°掉头 → 巡线 → 第1边沿90°转 → 400pwm循迹500ms
  ******************************************************************************
  */

#ifndef __TASK4_LINETRACK_H__
#define __TASK4_LINETRACK_H__

void Task4_LineTrack_Init(void);
void Task4_LineTrack_Loop(void);
void Task4_LineTrack_IRQHandler(void);

#endif /* __TASK4_LINETRACK_H__ */
