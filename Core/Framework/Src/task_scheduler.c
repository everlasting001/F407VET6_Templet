/**
 * @file task_scheduler.c
 * @brief 任务调度器实现
 *
 * 手动分频算法：
 * 每个任务有一个周期 (period_ms) 和计数器 (tick_count)
 * 每次 scheduler_tick() 调用时：
 *   1. 计数器 +1
 *   2. 如果计数器 >= 周期，复位计数器并执行任务
 */

#include "task_scheduler.h"
#include <stddef.h>

static Scheduler_t g_scheduler = {0};

void scheduler_init(void) {
    g_scheduler.num_tasks = 0;
    g_scheduler.total_ticks = 0;

    for (int i = 0; i < MAX_TASKS; i++) {
        g_scheduler.tasks[i].func = NULL;
        g_scheduler.tasks[i].period_ms = 0;
        g_scheduler.tasks[i].tick_count = 0;
        g_scheduler.tasks[i].enabled = 0;
    }
}

int scheduler_add_task(task_func_t func, uint16_t period_ms) {
    if (func == NULL || period_ms == 0) return -1;
    if (g_scheduler.num_tasks >= MAX_TASKS) return -1;

    int id = g_scheduler.num_tasks;
    g_scheduler.tasks[id].func = func;
    g_scheduler.tasks[id].period_ms = period_ms;
    g_scheduler.tasks[id].tick_count = 0;
    g_scheduler.tasks[id].enabled = 1;
    g_scheduler.num_tasks++;

    return id;
}

void scheduler_remove_task(int task_id) {
    if (task_id < 0 || task_id >= g_scheduler.num_tasks) return;

    g_scheduler.tasks[task_id].func = NULL;
    g_scheduler.tasks[task_id].enabled = 0;
}

void scheduler_enable_task(int task_id) {
    if (task_id >= 0 && task_id < g_scheduler.num_tasks) {
        g_scheduler.tasks[task_id].enabled = 1;
    }
}

void scheduler_disable_task(int task_id) {
    if (task_id >= 0 && task_id < g_scheduler.num_tasks) {
        g_scheduler.tasks[task_id].enabled = 0;
    }
}

void scheduler_tick(void) {
    g_scheduler.total_ticks++;

    for (int i = 0; i < g_scheduler.num_tasks; i++) {
        if (!g_scheduler.tasks[i].enabled || g_scheduler.tasks[i].func == NULL) {
            continue;
        }

        g_scheduler.tasks[i].tick_count++;
        if (g_scheduler.tasks[i].tick_count >= g_scheduler.tasks[i].period_ms) {
            g_scheduler.tasks[i].tick_count = 0;
            g_scheduler.tasks[i].func();
        }
    }
}

uint32_t scheduler_get_total_ticks(void) {
    return g_scheduler.total_ticks;
}

uint8_t scheduler_get_task_count(void) {
    return g_scheduler.num_tasks;
}
