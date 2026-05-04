/**
  ******************************************************************************
  * @file    LED.h
  * @brief   LED 模块子类 — 继承 ModuleBase 的 GPIO LED 控制实现
  *
  * @details
  * 本文件定义了 LED 模块的结构体和公有接口，继承自 ModuleBase 基类。
  * 基类提供 port 和 pin 属性，子类专注实现 LED 特有功能：
  *   - LED 开/关/翻转控制
  *   - 软件 PWM 亮度调节（0~100%）
  *   - 电平极性配置（高电平亮 / 低电平亮）
  *
  * === 使用示例 ===
  *
  * // 1. 定义并构造 LED 对象（PC0，高电平亮）
  * LED_t led;
  * LED_Constructor(&led, GPIOC, GPIO_PIN_0, 1);
  *
  * // 2. 通过基类接口初始化
  * ModuleBase_Init((ModuleBase_t *)&led);
  *
  * // 3. LED 控制
  * LED_On(&led);                      // 点亮
  * LED_Off(&led);                     // 熄灭
  * LED_Toggle(&led);                  // 翻转
  * LED_SetBrightness(&led, 50);       // 设置亮度 50%
  *
  * // 4. 通过基类多态运行/清理
  * ModuleBase_Run((ModuleBase_t *)&led);
  * ModuleBase_Cleanup((ModuleBase_t *)&led);
  *
  * @note   亮度调节使用软件 PWM（非阻塞，基于 HAL_GetTick 周期刷新），
  *         若需硬件 PWM 请自行配置定时器通道并修改实现。
  *
  ******************************************************************************
  */

#ifndef __LED_H__
#define __LED_H__

/* Includes ------------------------------------------------------------------*/
#include "ModuleBase.h"
#include <stdint.h>

/* ==================== LED 状态枚举 ==================== */

/**
  * @brief LED 开关状态枚举
  */
typedef enum {
    LED_STATE_OFF = 0,      /**< LED 熄灭 */
    LED_STATE_ON  = 1       /**< LED 点亮 */
} LED_State_t;

/* ==================== LED 结构体定义 ==================== */

/**
  * @brief LED 模块结构体（继承 ModuleBase）
  *
  * @note   ModuleBase_t 必须为第一个成员，确保指针可安全转换
  * @note   port 和 pin 由基类 ModuleBase_t 提供，子类无需重复定义
  */
typedef struct LED_s {
    ModuleBase_t  base;             /**< 基类（必须为第一个成员，含 port/pin）*/
    uint8_t       active_high;      /**< 电平极性: 1=高电平亮, 0=低电平亮 */
    uint8_t       state;            /**< 当前开关状态 (LED_State_t) */
    uint8_t       brightness;       /**< 当前亮度值 (0~100, 0=灭, 100=最亮) */
} LED_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  LED 构造函数
  * @note   初始化基类成员，通过 ModuleBase_SetPinPort() 设置 GPIO 端口和引脚。
  *         构造后默认熄灭，亮度为 100%。
  * @param  self         指向 LED 对象的指针
  * @param  port         LED 所在的 GPIO 端口（如 GPIOC）
  * @param  pin          LED 所在的 GPIO 引脚（如 GPIO_PIN_0）
  * @param  active_high  电平极性: 1 = 高电平亮, 0 = 低电平亮
  */
void LED_Constructor(LED_t *self, GPIO_TypeDef *port, uint16_t pin, uint8_t active_high);

/**
  * @brief  点亮 LED
  * @param  self  指向 LED 对象的指针
  */
void LED_On(LED_t *self);

/**
  * @brief  熄灭 LED
  * @param  self  指向 LED 对象的指针
  */
void LED_Off(LED_t *self);

/**
  * @brief  翻转 LED 状态（亮→灭 或 灭→亮）
  * @param  self  指向 LED 对象的指针
  */
void LED_Toggle(LED_t *self);

/**
  * @brief  设置 LED 亮度（软件 PWM）
  * @note   仅存储亮度值，由 Run() 中的非阻塞 PWM 状态机自动应用。
  *         PWM 周期固定为 10ms（100Hz），100 级分辨率。
  *         边界情况立即生效：brightness=0 立即熄灭，brightness=100 立即常亮。
  * @param  self        指向 LED 对象的指针
  * @param  brightness  亮度值 (0~100)
  *         - 0  : 完全熄灭
  *         - 100: 最亮（常亮）
  * @retval 0  成功
  * @retval -1 参数错误
  */
int LED_SetBrightness(LED_t *self, uint8_t brightness);

/**
  * @brief  获取当前 LED 开关状态
  * @param  self  指向 LED 对象的指针
  * @return LED_State_t  当前状态（LED_STATE_OFF / LED_STATE_ON）
  */
LED_State_t LED_GetState(const LED_t *self);

/**
  * @brief  获取当前 LED 亮度值
  * @param  self  指向 LED 对象的指针
  * @return uint8_t  当前亮度值 (0~100)
  */
uint8_t LED_GetBrightness(const LED_t *self);

#endif /* __LED_H__ */
