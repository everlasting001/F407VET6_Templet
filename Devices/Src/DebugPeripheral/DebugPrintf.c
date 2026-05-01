#include "DebugPrintf.h"
#include <stdio.h>

/* 格式化输出缓冲区大小（栈分配） */
#define DEBUG_PRINTF_FMT_BUF_SIZE  256

/* ==================== DebugPrintf 虚函数表 ==================== */

static int DebugPrintf_init(void *self)
{
    DebugPrintf_t *dbg = (DebugPrintf_t *)self;
    return UartBase_StartRx(&dbg->uart);
}

static const UartVTable_t debugprintf_vtable = {
    .init    = DebugPrintf_init,
    .run     = NULL,   /* 使用基类默认 */
    .cleanup = NULL,   /* 使用基类默认 */
};

/* ==================== 公有接口实现 ==================== */

void DebugPrintf_Constructor(DebugPrintf_t *self, UART_HandleTypeDef *huart,
                             uint8_t *rx_buffer, uint16_t rx_buf_size)
{
    UartBase_Constructor(&self->uart, "DebugPrintf", huart, rx_buffer, rx_buf_size);
    /* 替换为子类虚函数表 */
    self->uart.vtable = &debugprintf_vtable;
}

int DebugPrintf_Init(DebugPrintf_t *self)
{
    return UartBase_Init(&self->uart);
}

int DebugPrintf_Print(DebugPrintf_t *self, const char *fmt, ...)
{
    if (self == NULL || fmt == NULL)
        return -1;
    if (self->uart.tx_busy)
        return -1;  /* 上次发送未完成，丢弃（避免阻塞） */

    char buf[DEBUG_PRINTF_FMT_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0 || len >= (int)sizeof(buf)) {
        /* 格式化失败或截断，仅发送已截断部分 */
        len = (int)sizeof(buf) - 1;
    }

    return UartBase_SendDMA(&self->uart, (const uint8_t *)buf, (uint16_t)len);
}
