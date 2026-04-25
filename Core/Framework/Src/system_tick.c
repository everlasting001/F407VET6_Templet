/**
 * @file system_tick.c
 * @brief 系统滴答定时器实现
 */

#include "system_tick.h"
#include "task_scheduler.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>

static volatile uint32_t g_system_ticks = 0;

int system_tick_init(uint32_t frequency_hz) {
    uint32_t reload_value;

    if (frequency_hz == 0) return -1;

    reload_value = (HAL_RCC_GetHCLKFreq() / frequency_hz) - 1;

    if (HAL_SYSTICK_Config(reload_value) != HAL_OK) {
        return -1;
    }

    scheduler_init();
    g_system_ticks = 0;

    return 0;
}

void system_tick_update(void) {
    g_system_ticks++;
    scheduler_tick();
}

uint32_t system_get_ticks(void) {
    return g_system_ticks;
}

uint32_t system_elapsed_ms(uint32_t tick_start, uint32_t tick_end) {
    if (tick_end >= tick_start) {
        return tick_end - tick_start;
    } else {
        return (0xFFFFFFFFU - tick_start) + tick_end + 1;
    }
}

void system_tick_enable(void) {
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

void system_tick_disable(void) {
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}
