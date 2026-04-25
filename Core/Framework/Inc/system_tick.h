/**
 * @file system_tick.h
 * @brief 系统滴答定时器接口 - 基于 SysTick 的 1ms 中断
 *
 * 这个模块提供全局的系统时间戳和定时功能，与任务调度器配合使用。
 * 使用 ARM Cortex-M4 的 SysTick 定时器产生 1ms 中断。
 */

#ifndef SYSTEM_TICK_H
#define SYSTEM_TICK_H

#include <stdint.h>

/**
 * @brief 初始化系统滴答定时器
 * @param frequency_hz: 所需的定时频率（Hz），通常 1000 表示 1ms
 * @return 0 成功，-1 失败
 */
int system_tick_init(uint32_t frequency_hz);

/**
 * @brief 系统滴答更新 - 在 SysTick_Handler 中调用
 * 这个函数驱动任务调度器的更新
 */
void system_tick_update(void);

/**
 * @brief 获取当前系统时间戳（毫秒）
 * @return 系统运行时间（ms），溢出会自动回绕
 */
uint32_t system_get_ticks(void);

/**
 * @brief 计算两个时间戳之间的差异（毫秒），考虑溢出
 * @param tick_start: 开始时刻
 * @param tick_end: 结束时刻
 * @return 经过的毫秒数
 */
uint32_t system_elapsed_ms(uint32_t tick_start, uint32_t tick_end);

/**
 * @brief 启用/禁用系统滴答中断
 */
void system_tick_enable(void);
void system_tick_disable(void);

#endif // SYSTEM_TICK_H
