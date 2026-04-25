/**
 * @file debug_utils.h
 * @brief 调试工具函数声明
 *
 * 提供printf重定向、格式化输出等调试便利函数。
 * 配合 USART1 + DMA + IDLE 中断使用。
 *
 * 使用示例：
 * @code
 *   // 方式1：直接使用 printf（重定向到USART1）
 *   printf("PID: Kp=%.2f Ki=%.2f Kd=%.2f\r\n", kp, ki, kd);
 *
 *   // 方式2：使用 debug_printf（带时间戳前缀）
 *   debug_printf("Gyro: %.2f %.2f %.2f\r\n", gx, gy, gz);
 *
 *   // 方式3：使用 debug_hex_dump（打印二进制数据）
 *   debug_hex_dump("RX", data, len);
 * @endcode
 */

#ifndef __DEBUG_UTILS_H__
#define __DEBUG_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ==================== 调试输出级别 ==================== */

#define DEBUG_LEVEL_NONE    0   ///< 关闭所有调试输出
#define DEBUG_LEVEL_ERROR   1   ///< 仅输出错误信息
#define DEBUG_LEVEL_WARN    2   ///< 输出错误和警告
#define DEBUG_LEVEL_INFO    3   ///< 输出一般信息
#define DEBUG_LEVEL_DEBUG   4   ///< 输出详细调试信息
#define DEBUG_LEVEL_VERBOSE 5   ///< 输出所有信息

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_INFO  ///< 默认调试级别
#endif

/* ==================== 宏定义 ==================== */

/**
 * @brief 条件编译的调试打印宏
 *
 * 根据 DEBUG_LEVEL 控制编译，发布版本中自动移除调试代码。
 *
 * 用法：
 *   DEBUG_PRINT(DEBUG_LEVEL_INFO, "Sensor value: %d\r\n", val);
 */
#if DEBUG_LEVEL >= DEBUG_LEVEL_ERROR
#define DEBUG_ERROR(fmt, ...)  debug_printf("[ERR] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_ERROR(fmt, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_WARN
#define DEBUG_WARN(fmt, ...)   debug_printf("[WARN] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_WARN(fmt, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_INFO
#define DEBUG_INFO(fmt, ...)   debug_printf("[INFO] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_INFO(fmt, ...)
#endif

#if DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG
#define DEBUG_DEBUG(fmt, ...)  debug_printf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DEBUG_DEBUG(fmt, ...)
#endif

/* ==================== 函数原型 ==================== */

/**
 * @brief 格式化调试输出（带时间戳）
 *
 * 自动添加系统运行时间戳前缀，格式：
 * [123.456] message...
 *
 * @param fmt 格式化字符串，用法同 printf
 * @param ... 可变参数
 */
void debug_printf(const char *fmt, ...);

/**
 * @brief 简易调试输出（无格式，直接发送字符串）
 *
 * 用于快速输出常量字符串，不经过 vsnprintf 格式化
 * 以减少代码体积和处理时间。
 *
 * @param str 要输出的字符串
 */
void debug_puts(const char *str);

/**
 * @brief 十六进制数据打印（调试利器！）
 *
 * 将缓冲区数据按十六进制 + ASCII 格式打印。
 * 非常适合调试陀螺仪原始数据、通信协议等。
 *
 * 输出格式：
 *   [TAG] 0010:  1A 2B 3C 4D  5E 6F 78 90  |.+4M|_ox.|
 *
 * @param tag  数据标签（如 "GYRO", "PID"）
 * @param data 数据缓冲区
 * @param len  数据长度
 */
void debug_hex_dump(const char *tag, const uint8_t *data, uint16_t len);

/**
 * @brief 调试断言
 *
 * 条件不满足时输出错误信息并进入错误处理。
 *
 * @param expr  断言表达式
 * @param msg   失败时输出的信息
 */
#define DEBUG_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            debug_printf("[ASSERT] %s at %s:%d\r\n", msg, __FILE__, __LINE__); \
            Error_Handler(); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_UTILS_H__ */
