/**
 * @file debug_utils.c
 * @brief 调试工具函数实现
 *
 * 提供 printf 重定向和格式化调试输出功能。
 * 所有输出通过 USART1 + DMA 发送。
 *
 * ════════════════════════════════════════════════════════════
 *  printf 重定向原理
 * ════════════════════════════════════════════════════════════
 *
 * ARM GCC (Newlib) 中，printf 最终调用 _write() 系统调用。
 * 我们重写 _write() 将输出定向到 USART1：
 *
 *   printf("hello") → _write(1, "hello", 5) → uart_debug_send()
 *
 * 对于 ARMCC (MDK-ARM)，需重写 fputc()。
 * 对于 IAR，需重写 __write()。
 *
 * 本文件同时提供 ARM GCC 和 ARMCC 两种编译器支持。
 */

#include "debug_utils.h"
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ==================== printf 重定向 ==================== */

/**
 * @brief printf 重定向到 USART1（ARM GCC/Newlib）
 *
 * ARM GCC 工具链使用 Newlib 标准库。
 * printf 最终通过 _write() 系统调用输出字符。
 *
 * @param file 文件描述符（1=stdout, 2=stderr）
 * @param ptr  数据缓冲区
 * @param len  数据长度
 * @return int 实际写入的字节数
 */
int _write(int file, char *ptr, int len)
{
    (void)file;  /* 忽略文件描述符，全部输出到USART1 */

    if (ptr == NULL || len <= 0)
    {
        return 0;
    }

    uart_debug_send((uint8_t *)ptr, len);
    return len;
}

/* ==================== 调试输出函数 ==================== */

/**
 * @brief 格式化调试输出（带时间戳）
 *
 * 自动添加系统运行时间（毫秒），格式：
 * [123.456] message...
 *
 * 使用 vsnprintf 格式化，然后通过 DMA 发送。
 * 注意：格式化缓冲区固定为 UART_DEBUG_TX_BUF_SIZE 大小，
 *       超过此长度的输出会被截断。
 */
void debug_printf(const char *fmt, ...)
{
    if (fmt == NULL)
    {
        return;
    }

    char buf[UART_DEBUG_TX_BUF_SIZE];
    va_list args;
    int len;

    /* 添加时间戳前缀 */
    uint32_t tick = HAL_GetTick();
    len = snprintf(buf, sizeof(buf), "[%lu.%03lu] ",
                   (unsigned long)(tick / 1000),
                   (unsigned long)(tick % 1000));

    if (len < 0 || len >= (int)sizeof(buf))
    {
        return;  /* 格式化出错 */
    }

    /* 格式化用户消息 */
    va_start(args, fmt);
    int remaining = (int)sizeof(buf) - len;
    int msg_len = vsnprintf(buf + len, (size_t)remaining, fmt, args);
    va_end(args);

    if (msg_len < 0)
    {
        return;  /* 格式化出错 */
    }

    /* 计算实际总长度并发送 */
    int total_len = len + (msg_len < remaining ? msg_len : remaining - 1);
    uart_debug_send((uint8_t *)buf, (uint16_t)total_len);
}

/**
 * @brief 简易调试输出（无格式，直接发送字符串）
 *
 * 相比 debug_printf，不经过 vsnprintf 格式化，
 * 减少处理时间和代码体积，适合快速输出固定字符串。
 */
void debug_puts(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    uart_debug_send_str((char *)str);
}

/**
 * @brief 十六进制数据打印
 *
 * 以标准 hex dump 格式打印数据，每行16字节。
 * 非常适合调试通信协议、传感器原始数据等。
 *
 * 示例输出：
 *   [GYRO] 0000:  A1 B2 C3 D4 E5 F6 78 90  12 34 56 78 9A BC DE F0  |......x..4x.....|
 *   [GYRO] 0010:  01 02 03 04 05 06 07 08                           |........|
 */
void debug_hex_dump(const char *tag, const uint8_t *data, uint16_t len)
{
    if (tag == NULL || data == NULL || len == 0)
    {
        return;
    }

    char line_buf[128];  /* 每行：tag + offset + hex + ASCII */
    char ascii_buf[17];  /* ASCII 部分暂存 */
    uint16_t i;

    for (i = 0; i < len; i += 16)
    {
        uint16_t line_len = (len - i < 16) ? (len - i) : 16;
        int pos = 0;

        /* 写入标签和偏移量 */
        pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                        "[%s] %04X:  ", tag, i);

        /* 写入十六进制 */
        for (uint16_t j = 0; j < 16; j++)
        {
            if (j < line_len)
            {
                pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                                "%02X ", data[i + j]);
            }
            else
            {
                pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                                "   ");
            }

            /* 每8个字节加一个空格分隔 */
            if (j == 7)
            {
                pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                                " ");
            }
        }

        /* 写入 ASCII */
        pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos, " |");
        for (uint16_t j = 0; j < line_len; j++)
        {
            char c = (char)data[i + j];
            ascii_buf[j] = (c >= 0x20 && c <= 0x7E) ? c : '.';
        }
        ascii_buf[line_len] = '\0';
        pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                        "%s|", ascii_buf);

        /* 添加换行并发送 */
        pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos, "\r\n");
        uart_debug_send((uint8_t *)line_buf, (uint16_t)pos);
    }
}
