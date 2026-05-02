#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include "main.h"
#include "Init.h"

/* 定时分频标志（由 TIM2 1ms 中断设置，应用层查询并清除）*/
extern volatile uint8_t Flag_1ms;
extern volatile uint8_t Flag_2ms;
extern volatile uint8_t Flag_10ms;
extern volatile uint8_t Flag_40ms;
extern volatile uint8_t Flag_100ms;
extern volatile uint8_t Flag_500ms;
extern volatile uint8_t Flag_1000ms;

void Callback_Init(void);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#endif
