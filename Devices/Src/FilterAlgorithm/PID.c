/**
  ******************************************************************************
  * @file    PID.c
  * @brief   PID 控制器实现 — 位置式 PID + 抗积分饱和 + 输出限幅
  ******************************************************************************
  */

#include "PID.h"
#include <math.h>

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float integral_limit, float output_limit)
{
    if (pid == NULL) return;

    pid->kp             = kp;
    pid->ki             = ki;
    pid->kd             = kd;
    pid->integral       = 0.0f;
    pid->prev_error     = 0.0f;
    pid->integral_limit = integral_limit;
    pid->output_limit   = output_limit;
    pid->first_call     = 1;
}

float PID_Compute(PID_t *pid, float setpoint, float measurement, float dt)
{
    if (pid == NULL || dt <= 0.0f) return 0.0f;

    float error = setpoint - measurement;

    /* 比例项 */
    float p_term = pid->kp * error;

    /* 积分项（带限幅防止积分饱和）*/
    pid->integral += error * dt;
    if (pid->integral >  pid->integral_limit) pid->integral =  pid->integral_limit;
    if (pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;
    float i_term = pid->ki * pid->integral;

    /* 微分项（对测量值微分，避免微分冲击）
       首拍初始化 prev_error 为当前误差，导数=0 防止冲击 */
    float d_term;
    if (pid->first_call) {
        pid->prev_error = error;
        pid->first_call = 0;
        d_term = 0.0f;
    } else {
        float derivative = (pid->prev_error - error) / dt;
        pid->prev_error  = error;
        d_term = pid->kd * derivative;
    }

    /* 输出合成 + 限幅 */
    float output = p_term + i_term + d_term;
    if (output >  pid->output_limit) output =  pid->output_limit;
    if (output < -pid->output_limit) output = -pid->output_limit;

    return output;
}

void PID_Reset(PID_t *pid)
{
    if (pid == NULL) return;
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
    pid->first_call = 1;
}
