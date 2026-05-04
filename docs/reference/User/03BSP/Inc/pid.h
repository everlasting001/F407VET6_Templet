#ifndef __PID_H__
#define __PID_H__

#include "Config.h"
#include "Variable.h"

// 函数声明
void PID_Init(PID_Controller_t *pid, PID_Type_e type);
float PID_Calculate(float target, float feedback, PID_Controller_t *pid);
float PID_PositionCalculate(PID_Controller_t *pid);
float PID_IncrementCalculate(PID_Controller_t *pid);

// 辅助函数
void PID_SetTarget(PID_Controller_t *pid, float target);
void PID_SetFeedback(PID_Controller_t *pid, float feedback);
float PID_GetOutput(PID_Controller_t *pid);
float PID_GetError(PID_Controller_t *pid);

// 参数设置函数
void PID_SetParams(PID_Controller_t *pid, float kp, float ki, float kd);
void PID_SetIntegralLimit(PID_Controller_t *pid, float max_val, float min_val);
void PID_SetOutputLimit(PID_Controller_t *pid, float max_val, float min_val);
void PID_SetDeadzone(PID_Controller_t *pid, float deadzone);
void PID_SetIntegralSeparation(PID_Controller_t *pid, float threshold);
void PID_SetAntiWindup(PID_Controller_t *pid, uint8_t enable);
void PID_SetDerivativeFilter(PID_Controller_t *pid, uint8_t enable, float alpha);

#endif
