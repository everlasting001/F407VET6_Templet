/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

/* USER CODE BEGIN Private defines */

/**
 * @brief 调试串口DMA接收缓冲区大小
 *
 * 根据实际调试需求调整：
 * - PID参数调节：256字节足够接收一帧参数命令
 * - 陀螺仪数据输出：如需要长时间连续输出，可增大此值
 * - 注意：此缓冲区是全局数组，过大浪费RAM
 */
#define UART_DEBUG_RX_BUF_SIZE  256

/**
 * @brief 调试串口DMA发送缓冲区大小
 *
 * 用于 printf / debug_printf 的格式化输出缓冲区。
 * 512字节可容纳约10行调试信息。
 */
#define UART_DEBUG_TX_BUF_SIZE  512

/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/**
 * @brief 初始化调试串口（DMA + IDLE空闲中断模式）
 *
 * 调用此函数启动DMA+IDLE中断接收。
 * 必须在 MX_USART1_UART_Init() 之后调用。
 *
 * 初始化流程：
 * 1. 确保USART1已初始化完成
 * 2. 调用 HAL_UARTEx_ReceiveToIdle_DMA() 启动IDLE接收
 * 3. 之后接收数据由DMA自动搬运，IDLE中断触发回调
 */
void uart_debug_init(void);

/**
 * @brief 通过DMA发送数据（非阻塞）
 *
 * 使用 DMA 方式发送数据，发送完成后触发
 * HAL_UART_TxCpltCallback 回调。
 *
 * @param data 数据缓冲区指针
 * @param len  要发送的字节数
 */
void uart_debug_send(uint8_t *data, uint16_t len);

/**
 * @brief 通过DMA发送字符串（非阻塞）
 *
 * 自动计算字符串长度并调用 uart_debug_send。
 *
 * @param str 以'\0'结尾的字符串
 */
void uart_debug_send_str(char *str);

/**
 * @brief 获取DMA接收缓冲区指针
 * @return uint8_t* 接收缓冲区指针
 */
uint8_t* uart_get_rx_buffer(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

