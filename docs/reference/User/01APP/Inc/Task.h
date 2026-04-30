#ifndef __TASK_H__
#define __TASK_H__

#include "Config.h"
#include "BSP_Headers.h"
#include "COM_Headers.h"

void Task_Init(void);
void Task_Function(void);
void Task_Main(void);
void Task_Callback(void);
void Task_SendUart1(void);
void Task_Test(void);

#endif