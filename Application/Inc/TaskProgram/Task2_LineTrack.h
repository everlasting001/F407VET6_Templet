/**
  ******************************************************************************
  * @file    Task2_LineTrack.h
  * @brief   Task2 — 正方形边框顺时针循迹一圈 (100cm×100cm)
  *
  * @details
  * 基于 Framework 层 MoveControl 巡线状态机，实现对正方形边框的顺时针循迹。
  * 与 Task1 区别: 转弯方向为顺时针 (右转, Yaw → -90°)。
  *
  * === 使用模块 ===
  * - 八路灰度循迹传感器 (LineSensor) — 直线循线 + 路口检测
  * - 左右轮编码器 (Encoder) — 路口微调前进距离计量
  * - MPU6050 陀螺仪 (Gyro) — 直角转弯 Yaw 闭环控制
  * - 左右直流电机 (DCMotor) — PWM 差速驱动
  *
  * === 三分法结构 ===
  * - Task2_LineTrack_Init()       — main() USER CODE BEGIN 2, 初始化循迹参数
  * - Task2_LineTrack_Loop()       — main() USER CODE BEGIN 3, 状态监控与调试输出
  * - Task2_LineTrack_IRQHandler() — 中断回调 (预留)
  *
  ******************************************************************************
  */

#ifndef __TASK2_LINE_TRACK_H__
#define __TASK2_LINE_TRACK_H__

#include <stdint.h>

/* ==================== Part 1: 初始化 ==================== */

/**
  * @brief  Task2 初始化 — 配置顺时针循迹参数并启动巡线模式
  * @note   调用时机: main() 中 USER CODE BEGIN 2, Framework_Init() 之后
  *         设置 turn_direction = -1 实现顺时针 (右转) 巡线
  */
void Task2_LineTrack_Init(void);

/* ==================== Part 2: 主循环 ==================== */

/**
  * @brief  Task2 主循环 — 状态监控与调试输出
  * @note   调用时机: main() 中 USER CODE BEGIN 3, while(1) 循环中
  *         每 500ms 打印一次当前边数和传感器状态
  */
void Task2_LineTrack_Loop(void);

/* ==================== Part 3: 中断回调 ==================== */

/**
  * @brief  Task2 中断回调 — 预留
  * @note   当前循迹控制由 MoveControl_LineTrackUpdate() 在 TIM2 ISR 中执行
  */
void Task2_LineTrack_IRQHandler(void);

#endif /* __TASK2_LINE_TRACK_H__ */
