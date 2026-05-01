/**
  ******************************************************************************
  * @file    DebugPrintf.h
  * @brief   DebugPrintf — UartBase 子类，格式化调试输出
  *
  * @details
  * 继承自 UartBase，提供 vsnprintf 格式化输出。
  * 不重定向标准 printf（不使用 _write 钩子），
  * 不使用 malloc，格式化缓冲区在栈上分配。
  *
  * === 使用示例 ===
  *
  * static uint8_t rx_buf[256];
  * DebugPrintf_t dbg;
  *
  * DebugPrintf_Constructor(&dbg, &huart1, rx_buf, sizeof(rx_buf));
  * DebugPrintf_Init(&dbg);
  * DebugPrintf_Print(&dbg, "Value: %d\r\n", 42);
  *
  ******************************************************************************
  */

#ifndef __DEBUG_PRINTF_H__
#define __DEBUG_PRINTF_H__

#include "UartBase.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== DebugPrintf 结构体 ==================== */

typedef struct {
    UartBase_t uart;  /**< 继承 UartBase（必须是第一个成员） */
} DebugPrintf_t;

/* ==================== 公有接口 ==================== */

/**
  * @brief  DebugPrintf 构造函数
  * @param  self         指向 DebugPrintf_t 对象
  * @param  huart        HAL UART 句柄
  * @param  rx_buffer    DMA 接收缓冲区（全局/静态）
  * @param  rx_buf_size  接收缓冲区大小
  */
void DebugPrintf_Constructor(DebugPrintf_t *self, UART_HandleTypeDef *huart,
                             uint8_t *rx_buffer, uint16_t rx_buf_size);

/**
  * @brief  初始化 DebugPrintf（启动 DMA+IDLE 接收）
  * @param  self  指向 DebugPrintf_t 对象
  * @retval 0=成功, -1=失败
  */
int DebugPrintf_Init(DebugPrintf_t *self);

/**
  * @brief  格式化输出（通过 DMA 发送，非阻塞）
  * @note   使用 vsnprintf 格式化到栈缓冲区，再通过 DMA 发送。
  *         若前次发送未完成则丢弃本次数据（无排队）。
  * @param  self  指向 DebugPrintf_t 对象
  * @param  fmt   格式化字符串
  * @param  ...   可变参数
  * @retval 0=成功, -1=失败或忙
  */
int DebugPrintf_Print(DebugPrintf_t *self, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_PRINTF_H__ */
