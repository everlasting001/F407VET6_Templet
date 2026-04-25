/**
 * @file callback.h
 * @brief HAL库中断回调函数声明
 *
 * 本文件集中管理所有HAL库回调函数的声明。
 * 对应的实现在 callback.c 中。
 *
 * 使用方式：
 * 1. 本文件中的回调函数由HAL库自动调用（弱函数覆盖）
 * 2. 应用程序只需实现 HAL_UARTEx_RxEventCallback 中处理接收数据
 * 3. HAL_UARTEx_RxEventCallback 在 USART IDLE 空闲中断时被触发
 */

#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ==================== UART 回调函数 ==================== */

/**
 * @brief UART RX 完成回调（非IDLE模式下的普通DMA传输完成）
 * @param huart UART句柄指针
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief UART TX 完成回调（DMA发送完成时调用）
 * @param huart UART句柄指针
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief UART 错误回调（DMA传输出错时调用）
 * @param huart UART句柄指针
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

/**
 * @brief UART 扩展接收事件回调（IDLE空闲中断触发）
 *
 * 当使用 HAL_UARTEx_ReceiveToIdle_DMA() 启动接收后，
 * 检测到USART空闲线中断时，此回调被调用。
 *
 * @param huart UART句柄指针
 * @param Size  本次接收到的数据字节数
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

/* ==================== 应用层回调声明 ==================== */

/**
 * @brief 用户数据处理回调（由 HAL_UARTEx_RxEventCallback 内部调用）
 *
 * 应用程序实现此函数来处理接收到的调试串口数据。
 *
 * @param data 数据缓冲区指针
 * @param len  数据长度
 */
void uart_debug_data_handler(uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __CALLBACK_H__ */
