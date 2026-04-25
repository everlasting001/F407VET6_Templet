/**
 * @file callback.c
 * @brief HAL库中断回调函数实现
 *
 * 本文件集中实现所有HAL库回调函数，包括：
 * - UART DMA传输完成回调
 * - UART IDLE空闲中断回调（核心）
 * - UART错误处理回调
 *
 * ════════════════════════════════════════════════════════════
 *  DMA + IDLE 空闲中断接收工作原理：
 * ════════════════════════════════════════════════════════════
 *
 * 1. 调用 HAL_UARTEx_ReceiveToIdle_DMA() 启动DMA接收
 * 2. DMA在后台自动将USART接收到的数据搬运到缓冲区
 * 3. 当USART接收线空闲（无新数据超过1帧时间）时，硬件产生IDLE中断
 * 4. HAL库自动处理IDLE中断，调用 HAL_UARTEx_RxEventCallback()
 * 5. 在回调中获取已接收数据长度，处理数据，然后重新启动DMA接收
 *
 * 优点：
 * - 无需每接收一个字节就进一次中断，大大降低CPU负载
 * - 可接收任意长度的数据帧（无需预先知道数据长度）
 * - DMA自动搬运数据，CPU可处理其他任务
 *
 * ════════════════════════════════════════════════════════════
 */

#include "callback.h"
#include "usart.h"
#include "debug_utils.h"
#include <string.h>

/* ==================== 外部变量引用 ==================== */

/* ==================== 私有变量 ==================== */

/**
 * @brief UART TX DMA 传输完成标志
 * 由 HAL_UART_TxCpltCallback 设置
 */
static volatile uint8_t uart_tx_complete = 1;

/**
 * @brief UART RX 接收完成标志
 * 由 HAL_UARTEx_RxEventCallback 设置
 */
static volatile uint8_t uart_rx_ready = 0;

/**
 * @brief 上次IDLE中断接收到的数据长度
 */
static volatile uint16_t uart_last_rx_size = 0;

/* ==================== UART 回调实现 ==================== */

/**
 * @brief UART RX 完成回调（普通DMA传输完成，非IDLE模式）
 *
 * 此回调在以下情况被调用：
 * - 使用 HAL_UART_Receive_DMA() 且指定长度的数据接收完毕
 * - 注意：使用 HAL_UARTEx_ReceiveToIdle_DMA() 时不会触发此回调
 *         （IDLE中断会提前返回，由 RxEventCallback 处理）
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* 普通DMA接收完成（非IDLE模式） */
    }
}

/**
 * @brief UART TX 完成回调
 *
 * 当使用 HAL_UART_Transmit_DMA() 发送数据完成时调用。
 * 用于通知上层可以发送下一包数据。
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart_tx_complete = 1;
    }
}

/**
 * @brief UART 错误回调
 *
 * 当DMA传输过程中发生错误时调用。
 * 典型错误：溢出错误、帧错误、DMA传输错误等。
 *
 * 处理策略：重新初始化DMA接收，确保系统能继续工作。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* 清除错误状态 */
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);

        /* 停止并重启DMA接收 */
        HAL_UART_DMAStop(huart);

        /* 重新启动IDLE模式DMA接收 */
        HAL_UARTEx_ReceiveToIdle_DMA(huart, uart_get_rx_buffer(), UART_DEBUG_RX_BUF_SIZE);
        __HAL_UART_CLEAR_IDLEFLAG(huart);
    }
}

/**
 * @brief UART 扩展接收事件回调（核心函数）
 *
 * ════════════════════════════════════════════════════════════
 *  此回调是 DMA + IDLE 空闲中断的核心！
 * ════════════════════════════════════════════════════════════
 *
 * 触发条件：
 * 1. 已调用 HAL_UARTEx_ReceiveToIdle_DMA() 启动接收
 * 2. DMA正在后台连续接收数据
 * 3. USART RX线空闲超过1帧时间 → 硬件IDLE中断
 * 4. HAL库处理IDLE中断后调用此回调
 *
 * @param huart UART句柄
 * @param Size  从上次启动/重启接收到IDLE触发时，总共接收到的字节数
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        uint8_t *rx_buf = uart_get_rx_buffer();

        /* 保存接收信息 */
        uart_last_rx_size = Size;
        uart_rx_ready = 1;

        /* --- 在这里处理接收到的调试数据 --- */
        /* Size 指示接收了多少字节 */
        /* rx_buf 指向接收缓冲区，包含 Size 字节的有效数据 */

        if (Size > 0)
        {
            /* 调用应用层数据处理函数 */
            uart_debug_data_handler(rx_buf, Size);

            /* 清除接收缓冲区（可选，便于调试观察） */
            memset(rx_buf, 0, Size);
        }

        /* 重新启动DMA+IDLE接收，准备接收下一帧数据 */
        /* 注意：必须在处理完数据后立即重新启动 */
        HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, UART_DEBUG_RX_BUF_SIZE);
    }
}

/* ==================== 状态查询接口 ==================== */

/**
 * @brief 查询UART TX是否空闲
 * @return 1: 空闲可发送, 0: 正在发送
 */
uint8_t uart_is_tx_idle(void)
{
    return uart_tx_complete;
}

/**
 * @brief 查询是否收到新数据
 * @return 1: 有新数据, 0: 无新数据
 */
uint8_t uart_is_rx_ready(void)
{
    return uart_rx_ready;
}

/**
 * @brief 清除RX就绪标志（消费数据后调用）
 */
void uart_clear_rx_ready(void)
{
    uart_rx_ready = 0;
}

/**
 * @brief 获取上次接收的数据长度
 * @return 数据长度
 */
uint16_t uart_get_last_rx_size(void)
{
    return uart_last_rx_size;
}

/* ==================== 应用层数据处理（弱定义，用户可重写） ==================== */

/**
 * @brief 用户数据处理回调（弱定义）
 *
 * 应用程序可以在其他文件中重新实现此函数，
 * 以自定义对调试串口接收数据的处理逻辑。
 *
 * 默认实现：通过调试串口回显接收到的数据。
 *
 * @param data 数据缓冲区
 * @param len  数据长度
 */
__attribute__((weak)) void uart_debug_data_handler(uint8_t *data, uint16_t len)
{
    /* 默认行为：将接收到的数据回显 */
    /* 用户可以重写此函数实现自定义处理 */
    uart_debug_send(data, len);
}
