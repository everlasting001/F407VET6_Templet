/**
  ******************************************************************************
  * @file    Vofa.h
  * @brief   Vofa+ FireWater 协议调参模块 — 实时 PID 参数调整与状态监控
  *
  * @details
  * 通过 USART1 (共用 DebugPrintf 的 UartBase) 与 Vofa+ 上位机通信。
  *
  * === FireWater 协议 ===
  *
  * 上行 (MCU→Vofa), 13 通道 CSV:
  *   "Vofa:pos_Kp,pos_Ki,pos_Kd,vel_Kp,vel_Ki,vel_Kd,
  *    balance_Kp,balance_Kd,vel_l_rpm,vel_r_rpm,avg_dist,pos_error,target\r\n"
  *
  * 下行 (Vofa→MCU), Key:Value 格式:
  *   "pos_Kp:0.5\r\n"  设置位置环 Kp
  *   "pos_Ki:0.05\r\n" 设置位置环 Ki
  *   "pos_Kd:0.3\r\n"  设置位置环 Kd
  *   "vel_Kp:5.0\r\n"  设置速度环 Kp (左右共用)
  *   "vel_Ki:2.0\r\n"  设置速度环 Ki
  *   "vel_Kd:4.0\r\n"  设置速度环 Kd
  *   "balance_Kp:0.5\r\n" 设置差速修正比例系数
 *   "balance_Kd:1.0\r\n" 设置差速修正微分系数
  *   "Target:1000\r\n"  切换目标位置 (mm)
  *
  * === 集成步骤 ===
  *
  * 1. main.c USER CODE BEGIN 2: Vofa_Init(&dbg_printf.uart, &move_ctrl);
  * 2. main.c while 循环:     if(Flag_100ms) Vofa_SendTelemetry();  (10Hz)
  * 3. Callback.c HAL_UARTEx_RxEventCallback (USART1):
  *       UartBase_RxIdleCallback(&dbg_printf.uart, Size);
  *       Vofa_IRQHandler(&dbg_printf.uart);
  *
  ******************************************************************************
  */

#ifndef __VOFA_H__
#define __VOFA_H__

#include "MoveControl.h"
#include "UartBase.h"
#include "Gyro.h"

void Vofa_Init(UartBase_t *uart, MoveControl_t *ctrl);
void Vofa_SetGyro(Gyro_t *gyro);
void Vofa_SendTelemetry(void);
void Vofa_SendLineTrackTelemetry(void);
void Vofa_SendGyroTelemetry(void);
void Vofa_IRQHandler(UartBase_t *uart, uint16_t size);

#endif /* __VOFA_H__ */
