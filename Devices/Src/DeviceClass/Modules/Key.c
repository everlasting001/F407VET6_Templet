/**
  ******************************************************************************
  * @file    Key.c
  * @brief   按键模块子类实现 — 状态机驱动的按键事件检测
  *
  * @details
  * 提供 Key_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - 公有控制接口（Tick / Check / ClearFlag / GetRaw）
  *   - 5 状态 FSM（IDLE → PRESSED → RELEASED → DOUBLE_PRESSED → LONG_HOLD）
  *
  * === 状态机设计说明 ===
  *
  * 状态转移图：
  *   IDLE ──按下──▶ PRESSED ──超时(2s)──▶ LONG_HOLD ──周期(125ms)──▶ REPEAT
  *     ▲              │                       │
  *     │              │ 释放                   │ 释放
  *     │              ▼                       ▼
  *     │          RELEASED                  IDLE
  *     │              │
  *     │     ┌────────┴────────┐
  *     │     │ 超时(250ms)      │ 再次按下
  *     │     ▼                 ▼
  *     │  SINGLE           DOUBLE_PRESSED ──释放──▶ DOUBLE
  *     │     │                 │                      │
  *     └─────┴─────────────────┴──────────────────────┘
  *                     (均返回 IDLE)
  *
  * 去抖动：状态机每 KEY_DEBOUNCE_MS (20ms) 执行一次，忽略短于该时间的瞬态脉冲。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Key.h"
#include "stm32f4xx_hal.h"

/* ==================== 私有宏定义 ==================== */

#define KEY_DEBOUNCE_MS      20U    /**< 去抖动间隔 (ms) */
#define KEY_LONG_PRESS_MS    2000U  /**< 长按判定阈值 (ms) */
#define KEY_DOUBLE_MS        250U   /**< 双击窗口超时 (ms) */
#define KEY_REPEAT_MS        125U   /**< 长按连发间隔 (ms) */

/* ==================== FSM 状态枚举 ==================== */

typedef enum {
    KEY_FSM_IDLE           = 0,  /**< 空闲，等待按下 */
    KEY_FSM_PRESSED        = 1,  /**< 已按下，等待长按或释放 */
    KEY_FSM_RELEASED       = 2,  /**< 已释放，等待双击窗口或单击确认 */
    KEY_FSM_DOUBLE_PRESSED = 3,  /**< 双击的第二次按下，等待释放 */
    KEY_FSM_LONG_HOLD      = 4,  /**< 长按保持中，周期性产生连发 */
} Key_FsmState_t;

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  读取按键原始电平并转换为逻辑值
  * @param  key  指向 Key 对象的指针
  * @retval 0    按键释放（逻辑）
  * @retval 1    按键按下（逻辑）
  */
static inline uint8_t Key_ReadLogic(const Key_t *key)
{
    GPIO_PinState raw = HAL_GPIO_ReadPin(key->base.port, key->base.pin);
    if (key->active_low) {
        return (raw == GPIO_PIN_RESET) ? 1 : 0;
    } else {
        return (raw == GPIO_PIN_SET) ? 1 : 0;
    }
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  Key 初始化虚函数
  * @note   验证端口和引脚配置有效性，初始化状态机变量。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    参数无效
  */
static int Key_init(void *self)
{
    Key_t *key = (Key_t *)self;

    if ((key->base.port == NULL) || (key->base.pin == 0)) {
        return -1;
    }

    /* 读取初始电平作为 prev_raw */
    key->prev_raw    = Key_ReadLogic(key);
    key->event_flags = KEY_EVENT_NONE;
    key->fsm_state   = KEY_FSM_IDLE;
    key->tick_div    = 0;
    key->timer_cnt   = 0;

    return 0;
}

/**
  * @brief  Key 运行虚函数
  * @note   可选的轮询模式驱动。若已通过 ISR 调用 Key_Tick()，此函数仅返回 0。
  *         适用于无独立定时器 ISR 的场景。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int Key_run(void *self)
{
    (void)self;
    return 0;
}

/**
  * @brief  Key 清理虚函数
  * @note   清除所有事件标志，重置状态机。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int Key_cleanup(void *self)
{
    Key_t *key = (Key_t *)self;

    key->event_flags = KEY_EVENT_NONE;
    key->fsm_state   = KEY_FSM_IDLE;
    key->tick_div    = 0;
    key->timer_cnt   = 0;
    key->prev_raw    = 0;

    return 0;
}

/**
  * @brief  Key 复位虚函数
  * @note   复位到初始状态。
  * @param  self  指向模块对象自身的 void 指针
  */
static void Key_reset(void *self)
{
    Key_t *key = (Key_t *)self;

    key->event_flags = KEY_EVENT_NONE;
    key->fsm_state   = KEY_FSM_IDLE;
    key->tick_div    = 0;
    key->timer_cnt   = 0;
    key->prev_raw    = 0;
}

/* ==================== 子类虚函数表实例 ==================== */

static const ModuleVTable_t key_vtable = {
    .init    = Key_init,
    .run     = Key_run,
    .cleanup = Key_cleanup,
    .reset   = Key_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  Key 构造函数
  * @param  self        指向 Key 对象的指针
  * @param  port        Key 所在的 GPIO 端口
  * @param  pin         Key 所在的 GPIO 引脚
  * @param  active_low  电平极性: 1 = 低电平按下, 0 = 高电平按下
  */
void Key_Constructor(Key_t *self, GPIO_TypeDef *port, uint16_t pin, uint8_t active_low)
{
    if (self == NULL) {
        return;
    }

    ModuleBase_Constructor(&self->base, "KEY");
    ModuleBase_SetPinPort(&self->base, port, pin);

    self->active_low  = active_low;
    self->event_flags = KEY_EVENT_NONE;
    self->fsm_state   = KEY_FSM_IDLE;
    self->tick_div    = 0;
    self->timer_cnt   = 0;
    self->prev_raw    = 0;

    self->base.vtable = &key_vtable;
}

/**
  * @brief  按键状态机驱动
  * @note   调用方需确保每 1ms 调用一次。内部通过 tick_div 分频，
  *         实际状态机每 KEY_DEBOUNCE_MS (20ms) 执行一次。
  *
  *         事件产生逻辑：
  *         - DOWN/UP：电平边沿时立即设置
  *         - HOLD：每 tick 同步为当前电平状态
  *         - SINGLE：PRESSED 状态下释放后，250ms 内未再次按下即确认
  *         - DOUBLE：SINGLE 确认前再次按下→释放，确认双击
  *         - LONG_PRESS：按下持续超过 2000ms
  *         - REPEAT：长按期间每 125ms 产生一次
  *
  * @param  self  指向 Key 对象的指针
  */
void Key_Tick(Key_t *self)
{
    uint8_t curr_raw;

    if (self == NULL) {
        return;
    }

    /* 递减窗口定时器 (每 1ms) */
    if (self->timer_cnt > 0) {
        self->timer_cnt--;
    }

    /* 分频累加：每 1ms 调用，KEY_DEBOUNCE_MS ms 执行一次状态机 */
    self->tick_div++;
    if (self->tick_div < KEY_DEBOUNCE_MS) {
        return;
    }
    self->tick_div = 0;

    /* 读取当前电平并更新边沿 */
    curr_raw = Key_ReadLogic(self);

    /* HOLD 标志同步为当前电平 */
    if (curr_raw == 1) {
        self->event_flags |= KEY_EVENT_HOLD;
    } else {
        self->event_flags &= ~KEY_EVENT_HOLD;
    }

    /* DOWN / UP 边沿检测 */
    if (curr_raw == 1 && self->prev_raw == 0) {
        self->event_flags |= KEY_EVENT_DOWN;
    }
    if (curr_raw == 0 && self->prev_raw == 1) {
        self->event_flags |= KEY_EVENT_UP;
    }

    /* ==================== 状态机转移 ==================== */

    switch (self->fsm_state) {

    /* IDLE：等待按键按下 */
    case KEY_FSM_IDLE:
        if (curr_raw == 1) {
            self->timer_cnt = KEY_LONG_PRESS_MS;
            self->fsm_state = KEY_FSM_PRESSED;
        }
        break;

    /* PRESSED：按键按下中，等待释放或长按超时 */
    case KEY_FSM_PRESSED:
        if (curr_raw == 0) {
            /* 按键释放 → 进入双击窗口 */
            self->timer_cnt = KEY_DOUBLE_MS;
            self->fsm_state = KEY_FSM_RELEASED;
        } else if (self->timer_cnt == 0) {
            /* 长按超时 → 进入长按保持 */
            self->timer_cnt = KEY_REPEAT_MS;
            self->event_flags |= KEY_EVENT_LONG_PRESS;
            self->fsm_state = KEY_FSM_LONG_HOLD;
        }
        break;

    /* RELEASED：按键已释放，等待双击或单击确认 */
    case KEY_FSM_RELEASED:
        if (curr_raw == 1) {
            /* 窗口内再次按下 → 双击的第二按 */
            self->fsm_state = KEY_FSM_DOUBLE_PRESSED;
        } else if (self->timer_cnt == 0) {
            /* 窗口超时 → 确认单击 */
            self->event_flags |= KEY_EVENT_SINGLE;
            self->fsm_state = KEY_FSM_IDLE;
        }
        break;

    /* DOUBLE_PRESSED：双击的第二按保持中 */
    case KEY_FSM_DOUBLE_PRESSED:
        if (curr_raw == 0) {
            /* 第二次释放 → 确认双击 */
            self->event_flags |= KEY_EVENT_DOUBLE;
            self->fsm_state = KEY_FSM_IDLE;
        }
        break;

    /* LONG_HOLD：长按保持中，周期性产生连发 */
    case KEY_FSM_LONG_HOLD:
        /* 持续设置 LONG_PRESS（查询时不清除此标志，允许每 tick 重新读取）*/
        self->event_flags |= KEY_EVENT_LONG_PRESS;
        if (curr_raw == 0) {
            /* 释放 → 退出长按 */
            self->fsm_state = KEY_FSM_IDLE;
        } else if (self->timer_cnt == 0) {
            /* 连发周期到 */
            self->timer_cnt = KEY_REPEAT_MS;
            self->event_flags |= KEY_EVENT_REPEAT;
        }
        break;

    default:
        self->fsm_state = KEY_FSM_IDLE;
        break;
    }

    /* 保存当前电平供下次边沿检测 */
    self->prev_raw = curr_raw;
}

/**
  * @brief  查询指定按键事件是否发生
  * @note   除 KEY_EVENT_HOLD 外，查询后自动清除已查询到的事件位。
  *         支持组合查询多个事件。
  * @param  self   指向 Key 对象的指针
  * @param  event  要查询的事件标志
  * @retval 1      指定事件已发生
  * @retval 0      指定事件未发生
  */
uint8_t Key_Check(Key_t *self, Key_Event_t event)
{
    uint8_t matched;

    if (self == NULL) {
        return 0;
    }

    matched = (self->event_flags & (uint8_t)event) ? 1 : 0;

    if (matched) {
        if (event != KEY_EVENT_HOLD) {
            /* 非 HOLD 事件：查询后清除匹配位 */
            self->event_flags &= ~((uint8_t)event);
        }
        /* HOLD 事件不清除，反映当前持续状态 */
    }

    return matched;
}

/**
  * @brief  清除所有按键事件标志
  * @param  self  指向 Key 对象的指针
  */
void Key_ClearFlag(Key_t *self)
{
    if (self == NULL) {
        return;
    }
    self->event_flags = KEY_EVENT_NONE;
}

/**
  * @brief  读取按键原始 GPIO 电平（逻辑值）
  * @param  self  指向 Key 对象的指针
  * @retval 1     按键按下
  * @retval 0     按键释放
  */
uint8_t Key_GetRaw(const Key_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return Key_ReadLogic(self);
}

/**
  * @brief  判断按键是否处于按下状态
  * @param  self  指向 Key 对象的指针
  * @retval 1     按键按下中
  * @retval 0     按键释放中
  */
uint8_t Key_IsPressed(const Key_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return (self->event_flags & KEY_EVENT_HOLD) ? 1 : 0;
}
