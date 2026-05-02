/**
  ******************************************************************************
  * @file    PID.h
  * @brief   PID 控制器模块 — 位置式 PID，带抗积分饱和与输出限幅
  *
  * @details
  * 提供标准的 PID 控制算法，适用于速度环、位置环、差速修正等场景。
  *
  * === 使用示例 ===
  *
  * PID_t pid;
  * PID_Init(&pid, 1.0f, 0.1f, 0.05f, 100.0f, 200.0f);
  * float output = PID_Compute(&pid, target, actual, dt_s);
  * PID_Reset(&pid);  // 切换目标时清零历史
  *
  ******************************************************************************
  */

#ifndef __PID_H__
#define __PID_H__

#include <stdint.h>

typedef struct {
    float kp;              /**< 比例系数 */
    float ki;              /**< 积分系数 */
    float kd;              /**< 微分系数 */
    float integral;        /**< 积分累加值 */
    float prev_error;      /**< 上一次误差（用于微分计算）*/
    float integral_limit;  /**< 积分限幅（绝对值，防止积分饱和）*/
    float output_limit;    /**< 输出限幅（绝对值）*/
    uint8_t first_call;    /**< 首拍标志：1=需要初始化 prev_error */
} PID_t;

void  PID_Init(PID_t *pid, float kp, float ki, float kd,
               float integral_limit, float output_limit);
float PID_Compute(PID_t *pid, float setpoint, float measurement, float dt);
void  PID_Reset(PID_t *pid);

#endif /* __PID_H__ */
