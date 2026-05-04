/**
  ******************************************************************************
  * @file    Key.h
  * @brief   按键模块子类 — 继承 ModuleBase，支持单击/双击/长按/连发检测
  *
  * @details
  * 本文件定义了 Key 模块的结构体和公有接口，继承自 ModuleBase 基类。
  * 基类提供 port 和 pin 属性，子类实现按键状态机：
  *
  *   - 原始电平读取（支持 active_low 极性配置）
  *   - 按下/释放边沿检测（KEY_EVENT_DOWN / KEY_EVENT_UP）
  *   - 保持检测（KEY_EVENT_HOLD — 按键当前处于按下状态）
  *   - 单击检测（KEY_EVENT_SINGLE — 按下后释放，未触发双击）
  *   - 双击检测（KEY_EVENT_DOUBLE — 指定时间窗口内两次按下）
  *   - 长按检测（KEY_EVENT_LONG_PRESS — 按下超过阈值时间）
  *   - 连发检测（KEY_EVENT_REPEAT — 长按期间周期性触发）
  *
  * === 状态机时序说明 ===
  *
  *   IDLE ──按下──▶ PRESSED ──超时──▶ LONG_HOLD ──周期触发──▶ REPEAT
  *                    │                  │
  *                    │ 释放             │ 释放 → IDLE
  *                    ▼                  ▼
  *                RELEASED            IDLE
  *                    │
  *           ┌───────┴───────┐
  *           │ 超时          │ 再次按下
  *           ▼               ▼
  *       SINGLE(→IDLE)   DOUBLE_PRESSED ──释放──▶ DOUBLE(→IDLE)
  *
  * === 使用示例 ===
  *
  * // 1. 定义并构造 Key 对象（PC4，低电平按下）
  * Key_t key1;
  * Key_Constructor(&key1, GPIOC, GPIO_PIN_4, 1);
  * ModuleBase_Init((ModuleBase_t *)&key1);
  *
  * // 2. 在定时器 ISR 中周期性调用（每 1ms）
  * void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  *     if (htim == &htim2) {
  *         Key_Tick(&key1);
  *     }
  * }
  *
  * // 3. 在主循环中查询按键事件
  * if (Key_Check(&key1, KEY_EVENT_SINGLE)) {
  *     LED_Toggle(&led);
  * }
  * if (Key_Check(&key1, KEY_EVENT_LONG_PRESS)) {
  *     Buzzer_Beep(&buzzer, 100);
  * }
  *
  ******************************************************************************
  */

#ifndef __KEY_H__
#define __KEY_H__

/* Includes ------------------------------------------------------------------*/
#include "ModuleBase.h"
#include <stdint.h>

/* ==================== 按键事件标志枚举 ==================== */

/**
  * @brief 按键事件标志（可组合的位掩码）
  * @note  使用 Key_Check() 查询指定事件是否发生。
  *        除 KEY_EVENT_HOLD 外，查询后自动清除该事件位。
  *        KEY_EVENT_HOLD 反映当前按压状态，不会被自动清除。
  */
typedef enum {
    KEY_EVENT_NONE       = 0x00,   /**< 无事件 */
    KEY_EVENT_DOWN       = 0x01,   /**< 按键刚按下（边沿触发）*/
    KEY_EVENT_UP         = 0x02,   /**< 按键刚释放（边沿触发）*/
    KEY_EVENT_HOLD       = 0x04,   /**< 按键当前保持中（电平状态，不自动清除）*/
    KEY_EVENT_SINGLE     = 0x08,   /**< 单击完成（按下→释放，未触发双击）*/
    KEY_EVENT_DOUBLE     = 0x10,   /**< 双击完成（两次按下→释放）*/
    KEY_EVENT_LONG_PRESS = 0x20,   /**< 长按检测（按下超过阈值）*/
    KEY_EVENT_REPEAT     = 0x40,   /**< 连发触发（长按期间周期性产生）*/
} Key_Event_t;

/* ==================== Key 结构体定义 ==================== */

/**
  * @brief 按键模块结构体（继承 ModuleBase）
  *
  * @note   ModuleBase_t 必须为第一个成员，确保指针可安全转换
  * @note   port 和 pin 由基类 ModuleBase_t 提供
  */
typedef struct Key_s {
    ModuleBase_t  base;            /**< 基类（必须为第一个成员，含 port/pin）*/
    uint8_t       active_low;      /**< 电平极性: 1=低电平按下, 0=高电平按下 */
    uint8_t       event_flags;     /**< 当前活跃的事件标志位（Key_Event_t 位掩码）*/

    /* 状态机内部变量 */
    uint8_t       fsm_state;       /**< 当前状态机状态 */
    uint8_t       tick_div;        /**< 分频计数器 (累加到 KEY_DEBOUNCE_MS) */
    uint16_t      timer_cnt;       /**< 窗口定时器 (ms)，用于长按/双击超时 */
    uint8_t       prev_raw;        /**< 上一次原始电平 (0=释放, 1=按下) */
} Key_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  Key 构造函数
  * @note   初始化基类成员，设置电平极性，注册子类虚函数表。
  *         构造后状态机处于 IDLE，所有事件标志清零。
  * @param  self        指向 Key 对象的指针
  * @param  port        Key 所在的 GPIO 端口
  * @param  pin         Key 所在的 GPIO 引脚
  * @param  active_low  电平极性: 1 = 低电平按下, 0 = 高电平按下
  */
void Key_Constructor(Key_t *self, GPIO_TypeDef *port, uint16_t pin, uint8_t active_low);

/**
  * @brief  按键状态机驱动（每 1ms 调用一次）
  * @note   必须在定时器 ISR 或主循环中以 1ms 周期调用。
  *         内部以 KEY_DEBOUNCE_MS (20ms) 为间隔执行去抖动和状态转移。
  *         该函数不调用阻塞 API，可安全放在 ISR 中。
  *
  *         事件标志由状态机自动设置：
  *         - KEY_EVENT_DOWN / KEY_EVENT_UP：边沿触发，立即设置
  *         - KEY_EVENT_HOLD：每 tick 根据当前电平更新
  *         - KEY_EVENT_SINGLE / KEY_EVENT_DOUBLE：释放超时后设置
  *         - KEY_EVENT_LONG_PRESS：长按阈值到达时设置
  *         - KEY_EVENT_REPEAT：长按期间周期性设置
  *
  * @param  self  指向 Key 对象的指针
  */
void Key_Tick(Key_t *self);

/**
  * @brief  查询指定按键事件是否发生
  * @note   除 KEY_EVENT_HOLD 外，查询后自动清除该事件位。
  *         KEY_EVENT_HOLD 为持续状态，查询后不清除。
  *         支持组合查询（如 Key_Check(&k, KEY_EVENT_SINGLE | KEY_EVENT_DOUBLE)）。
  * @param  self   指向 Key 对象的指针
  * @param  event  要查询的事件标志（Key_Event_t 值或组合）
  * @retval 1      指定事件已发生
  * @retval 0      指定事件未发生
  */
uint8_t Key_Check(Key_t *self, Key_Event_t event);

/**
  * @brief  清除所有按键事件标志
  * @note   适用于模式切换等场景，防止上一模式的残留事件误触发。
  * @param  self  指向 Key 对象的指针
  */
void Key_ClearFlag(Key_t *self);

/**
  * @brief  读取按键原始 GPIO 电平
  * @note   根据 active_low 极性反转为逻辑值。
  * @param  self  指向 Key 对象的指针
  * @retval 1     按键按下
  * @retval 0     按键释放
  */
uint8_t Key_GetRaw(const Key_t *self);

/**
  * @brief  判断按键是否处于按下状态
  * @note   等同于检查 KEY_EVENT_HOLD 事件。
  * @param  self  指向 Key 对象的指针
  * @retval 1     按键按下中
  * @retval 0     按键释放中
  */
uint8_t Key_IsPressed(const Key_t *self);

#endif /* __KEY_H__ */
