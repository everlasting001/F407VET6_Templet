#include "DebugPrintf.h"
#include <stdio.h>
#include <string.h>

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
        return -1;

    char buf[DEBUG_PRINTF_FMT_BUF_SIZE];
    uint32_t tick = HAL_GetTick();

    /* 时间戳前缀: [s.ms] */
    int off = snprintf(buf, sizeof(buf), "[%lu.%03lu] ",
                       (unsigned long)(tick / 1000),
                       (unsigned long)(tick % 1000));

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf + off, sizeof(buf) - off, fmt, args);
    va_end(args);

    if (len < 0) {
        return -1;
    }
    int total = off + len;
    if (total >= (int)sizeof(buf)) {
        total = (int)sizeof(buf) - 1;
    }

    return UartBase_SendDMA(&self->uart, (const uint8_t *)buf, (uint16_t)total);
}

int DebugPrintf_HexDump(DebugPrintf_t *self, const char *label,
                        const uint8_t *data, uint16_t len)
{
    if (self == NULL || data == NULL || len == 0)
        return -1;

    /* 先发送标签头 */
    DebugPrintf_Print(self, "%s (%u bytes):\r\n", label, (unsigned int)len);

    char line[80];
    uint16_t offset = 0;

    while (offset < len) {
        uint16_t pos = 0;

        /* 十六进制部分: "  XX XX XX XX ..." */
        for (int i = 0; i < 16; i++) {
            if (offset + i < len) {
                pos += snprintf(line + pos, sizeof(line) - pos,
                                " %02X", data[offset + i]);
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            }
        }

        /* ASCII 部分 */
        pos += snprintf(line + pos, sizeof(line) - pos, "  |");
        for (int i = 0; i < 16 && (offset + i) < len; i++) {
            uint8_t c = data[offset + i];
            line[pos++] = (c >= 32 && c <= 126) ? (char)c : '.';
        }
        pos += snprintf(line + pos, sizeof(line) - pos, "|\r\n");

        UartBase_SendDMA(&self->uart, (const uint8_t *)line, (uint16_t)pos);

        offset += 16;
    }

    return 0;
}

/* ==================== printf 重定向 ==================== */

/**
  * @brief  _write 系统调用钩子 — 将 printf 输出重定向到 UART DMA
  * @note   Newlib 的 printf() 最终调用 _write(1, buf, len)。
  *         重写此函数即可将标准 printf 输出路由到 USART。
  */
int _write(int file, char *ptr, int len)
{
    (void)file;
    if (ptr == NULL || len <= 0) return 0;

    /* 使用全局 dbg_printf 实例发送 */
    if (UartBase_IsTxIdle(&dbg_printf.uart)) {
        UartBase_SendDMA(&dbg_printf.uart, (const uint8_t *)ptr, (uint16_t)len);
    }
    return len;
}
