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

/** @brief 灰度循迹采样分频系数 (默认 2 = 每2ms采样, Task3 设为 1 = 每1ms采样) */
extern volatile uint8_t g_line_track_divider;

void Callback_Init(void);
void Callback_SetLineTrackDivider(uint8_t divider);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c);

#endif
