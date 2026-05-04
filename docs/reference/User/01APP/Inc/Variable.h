#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include "Config.h"

/* 包含APP层使用的所有公共变量的声明以及初始化函数 */

extern encoder_t encoder[];

extern PID_Controller_t pid_yaw_controller;

extern pose_t pose;

extern Task_t task1, task2, task3, task4, task5, task6;

extern uint8_t Flag_SendUART1;
extern uint8_t Flag_ReceiveUART1;

extern uint8_t receiveData[50];
extern uint8_t SendData[256];

extern uint16_t ReceiveData_k230[256];
extern uint16_t SendData_k230[256];

extern uint8_t sensor_values[];

#endif