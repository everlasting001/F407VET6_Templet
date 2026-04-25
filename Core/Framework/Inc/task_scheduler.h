/**
 * @file task_scheduler.h
 * @brief 任务调度器接口 - 基于 1ms 系统滴答的软件任务调度
 *
 * 这个模块提供轻量级的周期性任务调度，无需 RTOS。
 * 采用手动分频计数器模式，每个任务有独立的周期和启用标志。
 *
 * 工作流：
 * 1. system_tick_init() 启动 SysTick 定时器
 * 2. scheduler_add_task() 注册需要的任务
 * 3. 每 1ms，SysTick 中断调用 system_tick_update()
 * 4. system_tick_update() 调用 scheduler_tick() 进行任务分发
 * 5. 满足周期条件的任务被调用
 */

#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <stdint.h>

#define MAX_TASKS 10  /**< 最大任务数 */

/**
 * @brief 任务函数类型
 */
typedef void (*task_func_t)(void);

/**
 * @brief 任务结构体
 */
typedef struct {
    task_func_t func;           /**< 任务函数指针 */
    uint16_t period_ms;         /**< 任务周期（毫秒） */
    uint16_t tick_count;        /**< 当前计数器（用于分频） */
    uint8_t enabled;            /**< 任务启用标志 */
} Task_t;

/**
 * @brief 调度器结构体
 */
typedef struct {
    Task_t tasks[MAX_TASKS];    /**< 任务数组 */
    uint8_t num_tasks;          /**< 当前任务数 */
    uint32_t total_ticks;       /**< 总系统滴答数（调试用） */
} Scheduler_t;

/**
 * @brief 初始化调度器
 */
void scheduler_init(void);

/**
 * @brief 添加一个周期性任务
 * @param func: 任务函数指针
 * @param period_ms: 任务周期（毫秒）
 * @return 任务 ID（0-MAX_TASKS-1），失败返回 -1
 *
 * @example
 * scheduler_add_task(motor_update, 5);      // 5ms 执行一次
 * scheduler_add_task(sensor_read, 10);      // 10ms 执行一次
 * scheduler_add_task(control_loop, 50);     // 50ms 执行一次
 */
int scheduler_add_task(task_func_t func, uint16_t period_ms);

/**
 * @brief 移除指定任务
 * @param task_id: 任务 ID
 */
void scheduler_remove_task(int task_id);

/**
 * @brief 启用/禁用指定任务
 */
void scheduler_enable_task(int task_id);
void scheduler_disable_task(int task_id);

/**
 * @brief 调度器更新 - 每 1ms 调用一次（在 SysTick 中断中）
 * 这个函数检查所有任务的周期计数器，
 * 如果任务的计数器达到周期要求，则调用该任务
 *
 * 手动分频逻辑：
 *   task->tick_count++;
 *   if (task->tick_count >= task->period_ms) {
 *       task->tick_count = 0;
 *       task->func();
 *   }
 */
void scheduler_tick(void);

/**
 * @brief 获取调度器信息（调试用）
 */
uint32_t scheduler_get_total_ticks(void);
uint8_t scheduler_get_task_count(void);

#endif // TASK_SCHEDULER_H
