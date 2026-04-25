/**
 * @file motor_driver.c
 * @brief DC 电机驱动实现
 */

#include "motor_driver.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"
#include <string.h>

static int motor_init_impl(Motor_t *motor);
static int motor_deinit_impl(Motor_t *motor);
static int motor_set_speed_impl(Motor_t *motor, uint16_t speed);
static int motor_set_direction_impl(Motor_t *motor, MotorDirection_e dir);
static int motor_stop_impl(Motor_t *motor);
static uint16_t motor_get_speed_impl(const Motor_t *motor);
static int motor_update_impl(Motor_t *motor);

static const MotorOps_t g_motor_ops = {
    .init = (int (*)(void *))motor_init_impl,
    .deinit = (int (*)(void *))motor_deinit_impl,
    .set_speed = (int (*)(void *, uint16_t))motor_set_speed_impl,
    .set_direction = (int (*)(void *, MotorDirection_e))motor_set_direction_impl,
    .stop = (int (*)(void *))motor_stop_impl,
    .get_speed = (uint16_t (*)(void *))motor_get_speed_impl,
    .update = (int (*)(void *))motor_update_impl,
};

int motor_init(Motor_t *motor,
               void *pwm_handle,
               uint32_t pwm_channel,
               void *gpio_port,
               uint32_t pin_fwd,
               uint32_t pin_bwd) {
    if (motor == NULL || pwm_handle == NULL) return -1;

    memset(motor, 0, sizeof(Motor_t));

    motor->base.name = "DC_Motor";
    motor->base.ops = (const DeviceOps_t *)&g_motor_ops;
    motor->ops = &g_motor_ops;

    motor->pwm_handle = pwm_handle;
    motor->pwm_channel = pwm_channel;
    motor->dir_port = gpio_port;
    motor->dir_pin_fwd = pin_fwd;
    motor->dir_pin_bwd = pin_bwd;

    return motor_init_impl(motor);
}

int motor_deinit(Motor_t *motor) {
    if (motor == NULL) return -1;
    return motor_deinit_impl(motor);
}

int motor_set_speed(Motor_t *motor, uint16_t speed) {
    if (motor == NULL) return -1;
    return motor_set_speed_impl(motor, speed);
}

int motor_set_direction(Motor_t *motor, MotorDirection_e direction) {
    if (motor == NULL) return -1;
    return motor_set_direction_impl(motor, direction);
}

int motor_stop(Motor_t *motor) {
    if (motor == NULL) return -1;
    return motor_stop_impl(motor);
}

uint16_t motor_get_speed(const Motor_t *motor) {
    if (motor == NULL) return 0;
    return motor_get_speed_impl(motor);
}

int motor_update(Motor_t *motor) {
    if (motor == NULL) return -1;
    return motor_update_impl(motor);
}

static int motor_init_impl(Motor_t *motor) {
    if (motor->pwm_handle == NULL) return -1;

    TIM_HandleTypeDef *htim = (TIM_HandleTypeDef *)motor->pwm_handle;
    HAL_TIM_PWM_Start(htim, motor->pwm_channel);

    motor->pwm_value = 0;
    motor->direction = MOTOR_STOP;
    motor->enabled = 1;
    motor->base.initialized = 1;

    return 0;
}

static int motor_deinit_impl(Motor_t *motor) {
    if (motor->pwm_handle == NULL) return -1;

    TIM_HandleTypeDef *htim = (TIM_HandleTypeDef *)motor->pwm_handle;
    HAL_TIM_PWM_Stop(htim, motor->pwm_channel);

    motor->enabled = 0;
    motor->base.initialized = 0;

    return 0;
}

static int motor_set_speed_impl(Motor_t *motor, uint16_t speed) {
    if (!motor->enabled) return -1;
    if (speed > 1000) speed = 1000;

    motor->pwm_value = speed;

    if (motor->pwm_handle == NULL) return -1;

    TIM_HandleTypeDef *htim = (TIM_HandleTypeDef *)motor->pwm_handle;
    uint32_t pulse = (htim->Init.Period * speed) / 1000;
    __HAL_TIM_SET_COMPARE(htim, motor->pwm_channel, pulse);

    return 0;
}

static int motor_set_direction_impl(Motor_t *motor, MotorDirection_e dir) {
    if (!motor->enabled) return -1;

    motor->direction = dir;

    if (motor->dir_port == NULL) return -1;

    GPIO_TypeDef *port = (GPIO_TypeDef *)motor->dir_port;

    switch (dir) {
        case MOTOR_FORWARD:
            HAL_GPIO_WritePin(port, motor->dir_pin_fwd, GPIO_PIN_SET);
            HAL_GPIO_WritePin(port, motor->dir_pin_bwd, GPIO_PIN_RESET);
            break;
        case MOTOR_BACKWARD:
            HAL_GPIO_WritePin(port, motor->dir_pin_fwd, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(port, motor->dir_pin_bwd, GPIO_PIN_SET);
            break;
        case MOTOR_STOP:
        default:
            HAL_GPIO_WritePin(port, motor->dir_pin_fwd, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(port, motor->dir_pin_bwd, GPIO_PIN_RESET);
            break;
    }

    return 0;
}

static int motor_stop_impl(Motor_t *motor) {
    motor->pwm_value = 0;
    motor_set_speed_impl(motor, 0);
    motor_set_direction_impl(motor, MOTOR_STOP);
    return 0;
}

static uint16_t motor_get_speed_impl(const Motor_t *motor) {
    return motor->pwm_value;
}

static int motor_update_impl(Motor_t *motor) {
    (void)motor;
    return 0;
}
