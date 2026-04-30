#include "Variable.h"
#include "Config.h"
#include <stdint.h>

encoder_t encoder[2];

PID_Controller_t pid_yaw_controller;

pose_t pose;

Task_t task1, task2, task3, task4, task5, task6;

uint8_t Flag_SendUART1 = 0;
uint8_t Flag_ReceiveUART1 = 0;

uint8_t receiveData[50];
uint8_t SendData[256];

uint16_t ReceiveData_k230[256];
uint16_t SendData_k230[256];

uint8_t sensor_values[8];
