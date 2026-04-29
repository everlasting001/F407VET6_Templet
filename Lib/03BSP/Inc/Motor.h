#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "Config.h"
#include "Variable.h"

void Motor_Init(void);
void Motor_SetPWM(int16_t PWM,MotorIndex_e motor);
void Motor_Stop(MotorIndex_e motor);
void Spin_Left(uint16_t PWM);
void Spin_Right(uint16_t PWM);

#endif