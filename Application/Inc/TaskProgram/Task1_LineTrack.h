/**
  ******************************************************************************
  * @file    Task1_LineTrack.h
  * @brief   Task1 — 正方形边框循迹一圈 (100cm×100cm)
  *
  * @details
  * 基于 Framework 层 MoveControl 巡线状态机，实现对正方形边框的完整循迹。
  * 小车在每条 100cm 直线上循线，到达直角拐角时通过灰度传感器检测路口，
  * 自动执行 90° 直角转弯（陀螺仪 Yaw 闭环）。
  * 完成 4 条边后进入减速循迹阶段（PWM 670→250 线性递减，距离 750mm），
  * 检测到第 5 个边沿时立即停车。
  *
  * === 使用模块 ===
  * - 八路灰度循迹传感器 (LineSensor) — 直线循线 + 路口检测
  * - 左右轮编码器 (Encoder) — 路口微调前进距离计量
  * - MPU6050 陀螺仪 (Gyro) — 直角转弯 Yaw 闭环控制
  * - 左右直流电机 (DCMotor) — PWM 差速驱动
  *
  * === 三分法结构 ===
  * - Task1_LineTrack_Init()       — main() USER CODE BEGIN 2, 初始化循迹参数
  * - Task1_LineTrack_Loop()       — main() USER CODE BEGIN 3, 状态监控与调试输出
  * - Task1_LineTrack_IRQHandler() — 中断回调 (预留，当前循迹在 MoveControl ISR 中执行)
  *
  ******************************************************************************
  */

#ifndef __TASK1_LINE_TRACK_H__
#define __TASK1_LINE_TRACK_H__

#include <stdint.h>

/* ==================== Part 1: 初始化 ==================== */

/**
  * @brief  Task1 初始化 — 配置循迹参数并启动巡线模式
  * @note   调用时机: main() 中 USER CODE BEGIN 2, Framework_Init() 之后
  *         配置 base_pwm / k_line / target_edges / intersection_threshold 等参数
  */
void Task1_LineTrack_Init(void);

/* ==================== Part 2: 主循环 ==================== */

/**
  * @brief  Task1 主循环 — 状态监控与调试输出
  * @note   调用时机: main() 中 USER CODE BEGIN 3, while(1) 循环中
  *         每 500ms 打印一次当前边数和传感器状态
  */
void Task1_LineTrack_Loop(void);

/* ==================== Part 3: 中断回调 ==================== */

/**
  * @brief  Task1 中断回调 — 预留
  * @note   当前循迹控制由 MoveControl_LineTrackUpdate() 在 TIM2 ISR 中执行，
  *         此函数预留用于未来按键启停等中断处理
  */
void Task1_LineTrack_IRQHandler(void);

#endif /* __TASK1_LINE_TRACK_H__ */
