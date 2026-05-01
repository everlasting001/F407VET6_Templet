/**
  ******************************************************************************
  * @file    DCMotor.h
  * @brief   DC 有刷电机子类 — TB6612FNG 驱动, 继承 MotorBase
  *
  * @details
  * 本文件定义了 DCMotor_t 结构体和公有接口，继承自 MotorBase 基类。
  * 子类负责管理:
  *   - AIN1/AIN2 方向控制引脚 (通过结构体数组传入)
  *   - TIM3 PWM 通道 (CH1/CH2, ARR=2099, 20kHz)
  *   - 方向极性 (CCW 为正, 可配置反转)
  *
  * === TB6612 方向真值表 (polarity=1, 默认) ===
  *
  * | AIN1 | AIN2 | PWMA | OUT1 | OUT2 | 电机状态     |
  * |------|------|------|------|------|-------------|
  * |  0   |  0   |  X   | 高阻 | 高阻 | 停止 (滑行)  |
  * |  1   |  0   | PWM  |  L   |  H   | 正转 (CCW)   |
  * |  0   |  1   | PWM  |  H   |  L   | 反转 (CW)    |
  * |  1   |  1   |  X   |  L   |  L   | 刹车         |
  *
  * === TIM3 PWM 参数 ===
  *
  * HCLK=168MHz → APB1 timer clk=84MHz → PSC=1(÷2) → 42MHz
  * ARR=2099 → PWM freq=42MHz/2100=20kHz, 分辨率 0~2099
  *
  * === 使用示例 ===
  *
  * // 1. 定义引脚配置 (以 Left_DCMotor 为例)
  * static const DCMotorPinConfig pins[DCMOTOR_PIN_COUNT] = {
  *     [DCMOTOR_PIN_AIN1] = {GPIOD, GPIO_PIN_0},   // AIN1→PD0
  *     [DCMOTOR_PIN_AIN2] = {GPIOD, GPIO_PIN_1},   // AIN2→PD1
  * };
  *
  * // 2. 构造并初始化
  * DCMotor_t motor;
  * DCMotor_Constructor(&motor, "Left_DCMotor", pins, &htim3, TIM_CHANNEL_1);
  * Motor_Init(&motor.base);
  *
  * // 3. 控制电机
  * DCMotor_SetSpeed(&motor, 1000);   // 正转 ~48% 占空比
  * DCMotor_SetSpeed(&motor, -500);   // 反转 ~24% 占空比
  * DCMotor_Stop(&motor);
  *
  * // 4. 主循环中运行
  * Motor_Run(&motor.base);
  *
  ******************************************************************************
  */

#ifndef __DCMOTOR_H__
#define __DCMOTOR_H__

/* Includes ------------------------------------------------------------------*/
#include "MotorBase.h"
#include "gpio.h"
#include "tim.h"
#include <stdint.h>

/* ==================== DC 电机常量 ==================== */

#define DCMOTOR_ARR_MAX             2099U   /**< TIM3 ARR (Period=2100-1) */

/* ==================== 引脚索引枚举 ==================== */

typedef enum {
    DCMOTOR_PIN_AIN1 = 0,  /**< 方向控制引脚 1 */
    DCMOTOR_PIN_AIN2 = 1,  /**< 方向控制引脚 2 */
    DCMOTOR_PIN_COUNT = 2
} DCMotorPinIndex;

/* ==================== 引脚配置结构体 ==================== */

typedef struct {
    GPIO_TypeDef *port;    /**< GPIO 端口 */
    uint16_t      pin;     /**< GPIO 引脚 */
} DCMotorPinConfig;

/* ==================== 方向枚举 ==================== */

typedef enum {
    DCMOTOR_DIR_FORWARD  = 0,  /**< 正转 (CCW, 逆时针) */
    DCMOTOR_DIR_BACKWARD = 1,  /**< 反转 (CW, 顺时针) */
} DCMotorDir;

/* ==================== DC 电机结构体定义 ==================== */

typedef struct DCMotor_s {
    MotorBase                base;           /**< 基类 (必须为第一个成员) */
    const DCMotorPinConfig  *pins;           /**< AIN1/AIN2 引脚配置数组指针 */
    TIM_HandleTypeDef       *htim;           /**< PWM 定时器句柄 (TIM3) */
    uint32_t                 tim_channel;    /**< PWM 定时器通道 (CH1/CH2) */
    int8_t                   polarity;       /**< 方向极性: 1=CCW正转, -1=CW正转 */
    uint16_t                 compare_value;  /**< 当前 PWM 比较值 (0~2099) */
    DCMotorDir               direction;      /**< 当前方向 */
} DCMotor_t;

/* ==================== 公有接口函数 ==================== */

void        DCMotor_Constructor(DCMotor_t *self, const char *name,
                                const DCMotorPinConfig *pins,
                                TIM_HandleTypeDef *htim, uint32_t tim_channel);
void        DCMotor_SetSpeed(DCMotor_t *self, int16_t speed);
void        DCMotor_Stop(DCMotor_t *self);
void        DCMotor_SetDirection(DCMotor_t *self, DCMotorDir dir);
uint16_t    DCMotor_GetSpeed(const DCMotor_t *self);
DCMotorDir  DCMotor_GetDirection(const DCMotor_t *self);

#endif /* __DCMOTOR_H__ */
