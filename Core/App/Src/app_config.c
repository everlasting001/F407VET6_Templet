/**
 * @file app_config.c
 * @brief 应用层初始化和主循环
 *
 * 这个文件演示如何使用框架层组件（任务调度器、状态机）
 * 来组织应用逻辑。
 *
 * 典型场景（电赛项目）：
 * - 定时读取传感器（10ms）
 * - 运行控制算法（50ms）
 * - 更新执行器（5ms）
 * - 主状态机管理整体流程
 */

#include "app_config.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 应用任务示例 1：传感器读取（10ms）
 */
static void app_task_sensor_read(void) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
}

/**
 * @brief 应用任务示例 2：控制循环（50ms）
 */
static void app_task_control_loop(void) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
}

/**
 * @brief 应用任务示例 3：电机更新（5ms）
 */
static void app_task_motor_update(void) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_2);
}

void app_init(void) {
    scheduler_add_task(app_task_sensor_read, 10);
    scheduler_add_task(app_task_control_loop, 50);
    scheduler_add_task(app_task_motor_update, 5);
}

void app_main_loop(void) {
    __WFI();
}
