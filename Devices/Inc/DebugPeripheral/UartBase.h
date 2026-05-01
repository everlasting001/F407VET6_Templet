/**
  ******************************************************************************
  * @file    UartBase.h
  * @brief   UART 基类 — DMA+IDLE 空闲中断收发
  *
  * @details
  * 本文件定义 UART 通信基类，使用 VTable 模式实现多态。
  * RX: DMA 循环接收 + IDLE 空闲中断（变长数据帧）
  * TX: DMA 发送 + TC 传输完成中断（非阻塞）
  *
  * === 继承关系 ===
  *   UartBase  (基类: init/run/cleanup, 收发逻辑)
  *      └── DebugPrintf  (子类: 格式化输出)
  *
  * === 使用示例 ===
  *
  * // 1. 定义接收缓冲区（全局/静态，DMA 要求）
  * static uint8_t rx_buf[256];
  *
  * // 2. 构造并初始化
  * UartBase_t uart;
  * UartBase_Constructor(&uart, "UART1", &huart1, rx_buf, sizeof(rx_buf));
  * UartBase_Init(&uart);
  *
  * // 3. DMA 发送（非阻塞）
  * UartBase_SendStr(&uart, "Hello\r\n");
  *
  * // 4. 检查接收（在 HAL_UARTEx_RxEventCallback 中更新）
  * if (uart.rx_done) {
  *     // 处理 uart.rx_buffer[0..uart.rx_len-1]
  *     uart.rx_done = 0;
  * }
  *
  ******************************************************************************
  */

#ifndef __UART_BASE_H__
#define __UART_BASE_H__

#include <stdint.h>
#include "usart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== UART 虚函数表 ==================== */

typedef struct {
    int (*init)(void *self);
    int (*run)(void *self);
    int (*cleanup)(void *self);
} UartVTable_t;

/* ==================== UART 基类结构体 ==================== */

typedef struct UartBase_s {
    const UartVTable_t *vtable;
    const char          *name;
    uint8_t              initialized;

    UART_HandleTypeDef  *huart;          /**< HAL UART 句柄 */
    uint8_t             *rx_buffer;      /**< DMA 接收缓冲区（全局/静态） */
    uint16_t             rx_buf_size;    /**< 接收缓冲区大小 */
    volatile uint16_t    rx_len;         /**< 本次接收到的字节数（IDLE 时更新） */
    volatile uint8_t     rx_done;        /**< 接收完成标志（IDLE 事件后置1，主循环清零） */
    volatile uint8_t     tx_busy;        /**< DMA 发送忙标志（1=发送中，0=空闲） */
    uint8_t              tx_buffer[256]; /**< DMA 发送缓冲区（避免栈缓冲区被覆盖） */
} UartBase_t;

/* ==================== 公有接口 ==================== */

/**
  * @brief  UART 构造函数
  * @param  self         指向 UartBase_t 对象
  * @param  name         模块名称
  * @param  huart        HAL UART 句柄（如 &huart1）
  * @param  rx_buffer    DMA 接收缓冲区（必须是全局/静态数组）
  * @param  rx_buf_size  接收缓冲区大小（字节）
  */
void UartBase_Constructor(UartBase_t *self, const char *name,
                          UART_HandleTypeDef *huart,
                          uint8_t *rx_buffer, uint16_t rx_buf_size);

/**
  * @brief  初始化 UART（启动 DMA+IDLE 接收）
  * @param  self  指向 UartBase_t 对象
  * @retval 0=成功, -1=失败
  */
int UartBase_Init(UartBase_t *self);

/**
  * @brief  运行 UART 主循环逻辑（当前预留）
  * @param  self  指向 UartBase_t 对象
  * @retval 0=成功
  */
int UartBase_Run(UartBase_t *self);

/**
  * @brief  清理 UART 资源（停止 DMA、关闭 IDLE 中断）
  * @param  self  指向 UartBase_t 对象
  * @retval 0=成功
  */
int UartBase_Cleanup(UartBase_t *self);

/**
  * @brief  DMA 发送数据（非阻塞）
  * @note   发送完成后 HAL_UART_TxCpltCallback 被调用。
  *         若前次发送未完成则返回 -1（不排队）。
  * @param  self  指向 UartBase_t 对象
  * @param  data  数据缓冲区指针
  * @param  len   发送字节数
  * @retval 0=成功, -1=忙
  */
int UartBase_SendDMA(UartBase_t *self, const uint8_t *data, uint16_t len);

/**
  * @brief  DMA 发送字符串（非阻塞）
  * @param  self  指向 UartBase_t 对象
  * @param  str   以 '\0' 结尾的字符串
  * @retval 0=成功, -1=忙
  */
int UartBase_SendStr(UartBase_t *self, const char *str);

/**
  * @brief  启动 DMA+IDLE 接收
  * @note   在构造/Init 中自动调用；
  *         也可在 HAL_UARTEx_RxEventCallback 中重新调用以持续接收。
  * @param  self  指向 UartBase_t 对象
  * @retval 0=成功, -1=失败
  */
int UartBase_StartRx(UartBase_t *self);

/**
  * @brief  IDLE 接收回调（由 HAL_UARTEx_RxEventCallback 调用）
  * @note   在 ISR 上下文中执行，保持快速。
  *         仅记录长度和置标志位，重启动 DMA 接收。
  * @param  self  指向 UartBase_t 对象
  * @param  len   接收到的字节数
  */
void UartBase_RxIdleCallback(UartBase_t *self, uint16_t len);

/**
  * @brief  TX 完成回调（由 HAL_UART_TxCpltCallback 调用）
  * @note   在 ISR 上下文中执行，保持快速。仅清除 tx_busy 标志。
  * @param  self  指向 UartBase_t 对象
  */
void UartBase_TxCpltCallback(UartBase_t *self);

/**
  * @brief  UART 错误回调（由 HAL_UART_ErrorCallback 调用）
  * @note   在 ISR 上下文中执行。清除 ORE/NE/FE/PE 错误标志，
  *         停止 DMA，重启 IDLE 接收以确保系统能继续工作。
  * @param  self  指向 UartBase_t 对象
  */
void UartBase_ErrorCallback(UartBase_t *self);

/* ==================== 状态查询接口 ==================== */

/**
  * @brief  查询 TX 是否空闲
  * @param  self  指向 UartBase_t 对象
  * @retval 1 = 空闲, 0 = 忙或参数无效
  */
uint8_t UartBase_IsTxIdle(const UartBase_t *self);

/**
  * @brief  查询是否有新接收数据
  * @param  self  指向 UartBase_t 对象
  * @retval 1 = 有新数据, 0 = 无数据或参数无效
  */
uint8_t UartBase_IsRxReady(const UartBase_t *self);

/**
  * @brief  消费接收数据后清除标志
  * @param  self  指向 UartBase_t 对象
  */
void UartBase_ClearRxReady(UartBase_t *self);

/**
  * @brief  获取上次接收的数据长度
  * @param  self  指向 UartBase_t 对象
  * @return 接收字节数（0 = 无数据或参数无效）
  */
uint16_t UartBase_GetLastRxSize(const UartBase_t *self);

/* ==================== 弱函数数据处理器 ==================== */

/**
  * @brief  接收数据处理函数（弱函数，应用层可覆盖）
  * @note   默认行为：回显接收到的数据。
  *         应用层可在任意 .c 文件中重新定义此函数以覆盖默认行为。
  * @param  self  指向 UartBase_t 对象
  * @param  data  接收数据缓冲区
  * @param  len   接收字节数
  */
void UartBase_DataHandler(UartBase_t *self, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __UART_BASE_H__ */
