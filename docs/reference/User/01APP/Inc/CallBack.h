#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include "BSP_Headers.h"
#include "Config.h"

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif