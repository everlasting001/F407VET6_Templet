/**
  ******************************************************************************
  * @file    Init.h
  * @brief   Framework 层全局初始化 — 统一管理所有模块的构建与调度
  *
  * @details
  * 本模块位于 Framework 层，负责:
  *   - 持有所有硬件模块的全局实例 (Encoder, DCMotor, DebugPrintf, ...)
  *   - 持有运动控制器 (MoveControl_t) 的全局实例
  *   - 提供 Framework_Init() 一站式初始化所有模块
  *   - 提供 Framework_IRQHandler() 供中断回调调用 (25Hz 周期调度)
  *
  * === 模块初始化顺序 ===
  *   1. DebugPrintf  — UART DMA 调试输出 (printf 重定向)
  *   2. Encoder      — 左右编码器 (TIM1/TIM8 编码器模式)
  *   3. DCMotor      — 左右电机 (TIM3 CH1/CH2 PWM)
  *   4. MoveControl  — 级联 PID 运动控制
  *
  * === 硬件接线 ===
  * 左电机编码器: TIM1_CH1 (PE9) + TIM1_CH2 (PE11)
  * 右电机编码器: TIM8_CH1 (PC6) + TIM8_CH2 (PC7)
  * 左电机驱动:   AIN1→PD0, AIN2→PD1, PWMA→PA6 (TIM3_CH1)
  * 右电机驱动:   BIN1→PD2, BIN2→PD3, PWMB→PA7 (TIM3_CH2)
  * 调试串口:     TX→PA9, RX→PA10 (USART1)
  *
  * === 使用示例 ===
  *
  *   // 在 main.c 中:
  *   // USER CODE BEGIN 2 — 外设初始化完成后
  *   Framework_Init();
  *
  *   // 在 Callback.c 中:
  *   // HAL_TIM_PeriodElapsedCallback → 40ms 软件分频
  *   if (Cnt_40ms >= 40) {
  *       Flag_40ms = 1;
  *       Framework_IRQHandler();
  *       Cnt_40ms = 0;
  *   }
  *
  ******************************************************************************
  */

#ifndef __FRAMEWORK_INIT_H__
#define __FRAMEWORK_INIT_H__

#include "Encoder.h"
#include "DCMotor.h"
#include "MoveControl.h"
#include "DebugPrintf.h"
#include "LineSensor.h"
#include "Gyro.h"

/* ==================== 全局实例 (extern) ==================== */

extern DebugPrintf_t  dbg_printf;     /**< 调试串口实例 (printf 重定向) */

extern Encoder_t      left_encoder;   /**< 左轮编码器 */
extern Encoder_t      right_encoder;  /**< 右轮编码器 */
extern DCMotor_t      left_motor;     /**< 左轮电机 */
extern DCMotor_t      right_motor;    /**< 右轮电机 */
extern MoveControl_t  move_ctrl;      /**< 运动控制器 */
extern LineSensor_t   line_sensor;    /**< 八路灰度循迹传感器 */
extern Gyro_t         gyro;           /**< MPU6050 陀螺仪 (I2C1) */

/* ==================== 公有接口 ==================== */

/**
  * @brief  Framework 层一站式初始化
  * @note   调用时机: main() 中 USER CODE BEGIN 2, 外设初始化完成后
  *         按顺序初始化所有硬件模块和运动控制算法
  */
void Framework_Init(void);

/**
  * @brief  Framework 层控制调度 (25Hz / 40ms)
  * @note   调用时机: main() while(1) 循环中, 由 Flag_40ms 门控
  *         编码器更新 (SensorBase_Run) 在 ISR 中完成,
  *         此函数仅执行级联 PID 控制计算与 PWM 输出。
  */
void Framework_IRQHandler(void);

#endif /* __FRAMEWORK_INIT_H__ */
