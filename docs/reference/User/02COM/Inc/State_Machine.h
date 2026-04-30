#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

#include "Variable.h"
#include "bsp_headers.h"
#include "Config.h"
#include <stdint.h>

void Task1_StateMachine(Task_t *task);
void Task1_Init(Task_t *task);

void Task2_StateMachine(Task_t *task);
void Task2_Init(Task_t *task);

void Task3_StateMachine(Task_t *task);
void Task3_Init(Task_t *task);

void Task4_StateMachine(Task_t *task);
void Task4_Init(Task_t *task);

#endif
