/**
  ******************************************************************************
  * @file    DebugPrintf.h
  * @brief   DebugPrintf — UartBase 子类，格式化调试输出
  *
  * @details
  * 继承自 UartBase，提供 vsnprintf 格式化输出。
  * 支持 printf 重定向（_write 钩子）、时间戳前缀、调试级别宏、
  * 十六进制 dump 等完整调试功能。
  * 不使用 malloc，格式化缓冲区在栈上分配。
  *
  * === 使用示例 ===
  *
  * static uint8_t rx_buf[256];
  * DebugPrintf_t dbg;
  *
  * DebugPrintf_Constructor(&dbg, &huart1, rx_buf, sizeof(rx_buf));
  * DebugPrintf_Init(&dbg);
  *
  * // 方式1: DebugPrintf_Print（带时间戳）
  * DebugPrintf_Print(&dbg, "Value: %d\r\n", 42);
  * // 输出: [12.345] Value: 42
  *
  * // 方式2: printf 重定向（通过 _write 钩子）
  * printf("Hello\r\n");
  *
  * // 方式3: 调试级别宏（可通过 DEBUG_LEVEL 编译开关控制）
  * DEBUG_INFO("System ready\r\n");
  * DEBUG_ERROR("Overcurrent! %d mA\r\n", current);
  *
  * // 方式4: 十六进制 dump
  * DebugPrintf_HexDump(&dbg, "GYRO", raw_data, 14);
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

/* ==================== 全局实例引用 ==================== */

/**
  * @brief 全局 DebugPrintf 实例（由应用层定义，如 DebugPrintfTest.c）
  * @note  调试级别宏（DEBUG_ERROR 等）通过此实例发送数据。
  *        应用层必须定义: DebugPrintf_t dbg_printf;
  */
extern DebugPrintf_t dbg_printf;

/* ==================== 调试级别定义 ==================== */

#define DEBUG_LEVEL_NONE   0
#define DEBUG_LEVEL_ERROR  1
#define DEBUG_LEVEL_WARN   2
#define DEBUG_LEVEL_INFO   3
#define DEBUG_LEVEL_DEBUG  4

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL  DEBUG_LEVEL_INFO
#endif

#if (DEBUG_LEVEL >= DEBUG_LEVEL_ERROR)
#define DEBUG_ERROR(fmt, ...)  DebugPrintf_Print(&dbg_printf, "[ERR] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_ERROR(fmt, ...)  ((void)0)
#endif

#if (DEBUG_LEVEL >= DEBUG_LEVEL_WARN)
#define DEBUG_WARN(fmt, ...)   DebugPrintf_Print(&dbg_printf, "[WARN] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_WARN(fmt, ...)   ((void)0)
#endif

#if (DEBUG_LEVEL >= DEBUG_LEVEL_INFO)
#define DEBUG_INFO(fmt, ...)   DebugPrintf_Print(&dbg_printf, "[INFO] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_INFO(fmt, ...)   ((void)0)
#endif

#if (DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG)
#define DEBUG_DEBUG(fmt, ...)  DebugPrintf_Print(&dbg_printf, "[DBG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_DEBUG(fmt, ...)  ((void)0)
#endif

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
  *         输出前缀自动添加时间戳 [秒.毫秒]。
  *         若前次发送未完成则丢弃本次数据（无排队）。
  * @param  self  指向 DebugPrintf_t 对象
  * @param  fmt   格式化字符串
  * @param  ...   可变参数
  * @retval 0=成功, -1=失败或忙
  */
int DebugPrintf_Print(DebugPrintf_t *self, const char *fmt, ...);

/**
  * @brief  十六进制 dump 输出
  * @note   格式化输出二进制数据的十六进制和 ASCII 视图。
  *         输出格式: "LABEL (N bytes):\r\n"
  *                   "  XX XX XX XX XX XX XX XX  |ASCII...|\r\n"
  * @param  self  指向 DebugPrintf_t 对象
  * @param  label 数据标签
  * @param  data  数据缓冲区
  * @param  len   数据字节数
  * @retval 0=成功, -1=失败或忙
  */
int DebugPrintf_HexDump(DebugPrintf_t *self, const char *label,
                        const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_PRINTF_H__ */
