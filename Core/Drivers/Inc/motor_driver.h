/**
 * @file motor_driver.h
 * @brief DC 有刷电机驱动 - 面向对象设计示例
 *
 * 这个模块演示如何使用虚函数指针表（vtable）实现面向对象的驱动设计。
 * 支持 PWM 速度控制和 GPIO 方向控制。
 *
 * 硬件接口：
 * - PWM 输出：控制电机速度
 * - GPIO 输出：控制电机方向（正/反转/停止）
 * - ADC 输入（可选）：电流反馈
 */

#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdint.h>
#include "device_base.h"

/**
 * @brief 电机方向枚举
 */
typedef enum {
    MOTOR_STOP = 0,         /**< 停止 */
    MOTOR_FORWARD = 1,      /**< 正转 */
    MOTOR_BACKWARD = 2,     /**< 反转 */
} MotorDirection_e;

/**
 * @brief 电机操作接口（虚函数表）
 */
typedef struct {
    int (*init)(void *dev);
    int (*deinit)(void *dev);
    int (*set_speed)(void *dev, uint16_t speed);        /**< 0-1000 */
    int (*set_direction)(void *dev, MotorDirection_e dir);
    int (*stop)(void *dev);
    uint16_t (*get_speed)(void *dev);
    int (*update)(void *dev);  /**< 周期更新，检查过流等 */
} MotorOps_t;

/**
 * @brief 电机驱动结构体
 */
typedef struct {
    Device_t base;              /**< 基类 */
    const MotorOps_t *ops;      /**< 操作接口指针 */

    uint16_t pwm_value;         /**< 当前 PWM 占空比（0-1000） */
    MotorDirection_e direction; /**< 当前方向 */
    uint8_t enabled;            /**< 使能标志 */

    void *pwm_handle;           /**< PWM 外设句柄（指向 TIM_HandleTypeDef） */
    uint32_t pwm_channel;       /**< PWM 通道 */
    void *dir_port;             /**< 方向控制 GPIO 端口 */
    uint32_t dir_pin_fwd;       /**< 正转引脚 */
    uint32_t dir_pin_bwd;       /**< 反转引脚 */
} Motor_t;

/**
 * @brief 初始化电机驱动
 * @param motor: 电机对象指针
 * @param pwm_handle: PWM 定时器句柄
 * @param pwm_channel: PWM 通道
 * @param gpio_port: GPIO 端口（用于方向控制）
 * @param pin_fwd: 正转引脚
 * @param pin_bwd: 反转引脚
 * @return 0 成功，-1 失败
 */
int motor_init(Motor_t *motor,
               void *pwm_handle,
               uint32_t pwm_channel,
               void *gpio_port,
               uint32_t pin_fwd,
               uint32_t pin_bwd);

/**
 * @brief 销毁电机驱动
 */
int motor_deinit(Motor_t *motor);

/**
 * @brief 设置电机速度
 * @param motor: 电机对象指针
 * @param speed: 速度值（0-1000，0 表示停止，1000 表示全速）
 * @return 0 成功，-1 失败
 */
int motor_set_speed(Motor_t *motor, uint16_t speed);

/**
 * @brief 设置电机方向
 * @param motor: 电机对象指针
 * @param direction: 方向（MOTOR_STOP/MOTOR_FORWARD/MOTOR_BACKWARD）
 * @return 0 成功，-1 失败
 */
int motor_set_direction(Motor_t *motor, MotorDirection_e direction);

/**
 * @brief 停止电机
 */
int motor_stop(Motor_t *motor);

/**
 * @brief 获取电机当前速度
 * @return 当前速度值（0-1000）
 */
uint16_t motor_get_speed(const Motor_t *motor);

/**
 * @brief 电机周期更新
 * 在任务调度中定期调用（如 5ms），用于过流检测等
 */
int motor_update(Motor_t *motor);

#endif // MOTOR_DRIVER_H
