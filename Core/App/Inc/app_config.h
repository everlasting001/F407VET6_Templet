/**
 * @file app_config.h
 * @brief 应用层配置 - 演示如何集成框架
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "system_tick.h"
#include "task_scheduler.h"
#include "state_machine.h"

/**
 * @brief 应用初始化
 * 在 main() 中调用此函数来初始化所有应用层组件
 */
void app_init(void);

/**
 * @brief 应用主循环
 * 在 main() 的 while(1) 中调用（或让 ISR 驱动所有任务）
 */
void app_main_loop(void);

#endif // APP_CONFIG_H
