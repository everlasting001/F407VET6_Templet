/**
  ******************************************************************************
  * @file    LED.c
  * @brief   LED 模块子类实现 — GPIO LED 控制（含软件 PWM 亮度调节）
  *
  * @details
  * 提供 LED_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - 公有控制接口（On / Off / Toggle / SetBrightness）
  *   - 非阻塞软件 PWM（在 Run 中周期性刷新）
  *
  * === 软件 PWM 设计说明 ===
  *
  * 由于 LED 引脚（PC0）未连接硬件 PWM 定时器，亮度调节采用软件 PWM：
  *   - PWM 周期: 10ms（100Hz）
  *   - 分辨率: 100 级（每级 100us）
  *   - 实现方式: 在 Run() 中通过 HAL_GetTick() 非阻塞刷新
  *
  * 若需硬件 PWM，可在子类扩展中添加定时器通道配置。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "LED.h"
#include "stm32f4xx_hal.h"      /* HAL_GPIO_WritePin, HAL_GetTick */

/* ==================== 私有宏定义 ==================== */

/**
  * @defgroup LED_Private_Macros 私有宏
  * @{
  */
#define LED_PWM_PERIOD_MS       10      /**< 软件 PWM 周期 (ms) */
#define LED_PWM_MAX_BRIGHTNESS  100U    /**< 亮度最大值 */
#define LED_PWM_MIN_BRIGHTNESS  0U      /**< 亮度最小值 */
/**
  * @}
  */

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  根据电平极性写入 GPIO
  * @note   封装了 active_high 逻辑，统一控制接口。
  * @param  led   指向 LED 对象的指针（非空）
  * @param  on    1 = 点亮, 0 = 熄灭
  */
static inline void LED_WritePin(LED_t *led, uint8_t on)
{
    GPIO_PinState pin_state;

    /* 根据电平极性决定 GPIO 输出电平 */
    if (led->active_high) {
        pin_state = (on) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        pin_state = (on) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    /* port 和 pin 继承自基类 ModuleBase_t */
    HAL_GPIO_WritePin(led->base.port, led->base.pin, pin_state);
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  LED 初始化虚函数
  * @note   验证端口和引脚配置的有效性，确保引脚初始为熄灭状态。
  *         若引脚已在 MX_GPIO_Init() 中配置，此处仅做状态确认。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    参数无效（端口或引脚为 0）
  */
static int LED_init(void *self)
{
    LED_t *led = (LED_t *)self;

    /* 参数完整性检查（port/pin 继承自基类）*/
    if ((led->base.port == NULL) || (led->base.pin == 0)) {
        return -1;
    }

    /* 初始状态：熄灭 */
    LED_WritePin(led, 0);
    led->state      = LED_STATE_OFF;
    led->brightness = LED_PWM_MAX_BRIGHTNESS;

    return 0;
}

/**
  * @brief  LED 运行虚函数 — 非阻塞软件 PWM 刷新
  * @note   在 main 循环中周期性调用，维持 LED 亮度。
  *         实现原理：在一个 PWM 周期内，根据 brightness 计算高/低电平持续时间，
  *         使用 HAL_GetTick() 实现非阻塞切换。
  *
  *         时序示例（brightness = 30, 周期 10ms）：
  *         [0ms ~ 3ms]  高电平（亮）
  *         [3ms ~ 10ms] 低电平（灭）
  *
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int LED_run(void *self)
{
    LED_t *led = (LED_t *)self;

    /* LED 关闭或亮度为 0：确保熄灭 */
    if ((led->state == LED_STATE_OFF) || (led->brightness == 0)) {
        LED_WritePin(led, 0);
        return 0;
    }

    /* 亮度 100%：常亮 */
    if (led->brightness >= LED_PWM_MAX_BRIGHTNESS) {
        LED_WritePin(led, 1);
        return 0;
    }

    /* === 软件 PWM：非阻塞状态机 === */

    /* 计算高电平持续时间（ms） */
    uint32_t high_ticks = (uint32_t)led->brightness *
                          LED_PWM_PERIOD_MS / LED_PWM_MAX_BRIGHTNESS;
    /* 低电平持续时间 */
    uint32_t low_ticks  = LED_PWM_PERIOD_MS - high_ticks;

    /* 获取当前系统滴答计数（单位 ms） */
    uint32_t now = HAL_GetTick();
    /* PWM 周期起始时刻 = 当前时刻对周期取模 */
    uint32_t cycle_pos = now % LED_PWM_PERIOD_MS;

    if (cycle_pos < high_ticks) {
        /* 高电平区间：点亮 */
        LED_WritePin(led, 1);
    } else {
        /* 低电平区间：熄灭 */
        LED_WritePin(led, 0);
    }

    (void)low_ticks;  /* low_ticks 用于可读性，未直接使用 */

    return 0;
}

/**
  * @brief  LED 清理虚函数
  * @note   熄灭 LED，重置状态和亮度为默认值。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int LED_cleanup(void *self)
{
    LED_t *led = (LED_t *)self;

    /* 熄灭 LED */
    LED_WritePin(led, 0);
    led->state      = LED_STATE_OFF;
    led->brightness = LED_PWM_MAX_BRIGHTNESS;

    return 0;
}

/**
  * @brief  LED 复位虚函数
  * @note   复位到初始状态：熄灭，亮度 100%。
  * @param  self  指向模块对象自身的 void 指针
  */
static void LED_reset(void *self)
{
    LED_t *led = (LED_t *)self;

    /* 熄灭 LED */
    LED_WritePin(led, 0);
    led->state      = LED_STATE_OFF;
    led->brightness = LED_PWM_MAX_BRIGHTNESS;
}

/* ==================== 子类虚函数表实例 ==================== */

/**
  * @brief LED 模块虚函数表
  * @note  所有虚函数均被重写，提供完整的 LED 控制行为。
  */
static const ModuleVTable_t led_vtable = {
    .init    = LED_init,
    .run     = LED_run,
    .cleanup = LED_cleanup,
    .reset   = LED_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  LED 构造函数
  * @param  self         指向 LED 对象的指针
  * @param  port         LED 所在的 GPIO 端口
  * @param  pin          LED 所在的 GPIO 引脚
  * @param  active_high  电平极性: 1 = 高电平亮, 0 = 低电平亮
  */
void LED_Constructor(LED_t *self, GPIO_TypeDef *port, uint16_t pin, uint8_t active_high)
{
    if (self == NULL) {
        return;
    }

    /* 1. 调用基类构造函数 */
    ModuleBase_Constructor(&self->base, "LED");

    /* 2. 设置 GPIO 端口和引脚到基类属性中 */
    ModuleBase_SetPinPort(&self->base, port, pin);

    /* 3. 初始化 LED 子类特有成员 */
    self->active_high = active_high;
    self->state       = LED_STATE_OFF;
    self->brightness  = LED_PWM_MAX_BRIGHTNESS;

    /* 4. 替换为子类虚函数表 */
    self->base.vtable = &led_vtable;
}

/**
  * @brief  点亮 LED
  * @param  self  指向 LED 对象的指针
  */
void LED_On(LED_t *self)
{
    if (self == NULL) {
        return;
    }

    self->state = LED_STATE_ON;

    /* 亮度为 100% 时直接常亮，否则由 Run() 中的 PWM 控制 */
    if (self->brightness >= LED_PWM_MAX_BRIGHTNESS) {
        LED_WritePin(self, 1);
    }
    /* 亮度 < 100% 时，下一次 Run() 调用会自动应用 PWM */
}

/**
  * @brief  熄灭 LED
  * @param  self  指向 LED 对象的指针
  */
void LED_Off(LED_t *self)
{
    if (self == NULL) {
        return;
    }

    self->state = LED_STATE_OFF;
    LED_WritePin(self, 0);
}

/**
  * @brief  翻转 LED 状态
  * @param  self  指向 LED 对象的指针
  */
void LED_Toggle(LED_t *self)
{
    if (self == NULL) {
        return;
    }

    if (self->state == LED_STATE_ON) {
        LED_Off(self);
    } else {
        LED_On(self);
    }
}

/**
  * @brief  设置 LED 亮度（软件 PWM）
  * @note   设置亮度值并存入对象中，由 Run() 中的非阻塞 PWM 状态机应用。
  *         若 LED 当前为点亮状态且 brightness = 0，则立即熄灭；
  *         若 brightness = 100，则立即常亮。
  *
  * @param  self        指向 LED 对象的指针
  * @param  brightness  亮度值 (0~100)
  *         - 0  : 完全熄灭
  *         - 100: 最亮（常亮）
  *
  * @retval 0  成功
  * @retval -1 参数错误
  */
int LED_SetBrightness(LED_t *self, uint8_t brightness)
{
    if (self == NULL) {
        return -1;
    }

    /* 限幅处理 */
    if (brightness > LED_PWM_MAX_BRIGHTNESS) {
        brightness = LED_PWM_MAX_BRIGHTNESS;
    }

    self->brightness = brightness;

    /* 若 LED 处于点亮状态，立即应用亮度变化 */
    if (self->state == LED_STATE_ON) {
        if (brightness == 0) {
            /* 亮度为 0：熄灭 */
            LED_WritePin(self, 0);
        } else if (brightness >= LED_PWM_MAX_BRIGHTNESS) {
            /* 亮度 100%：常亮 */
            LED_WritePin(self, 1);
        }
        /* 0 < brightness < 100%：由下一次 Run() 中的 PWM 控制 */
    }

    return 0;
}

/**
  * @brief  获取当前 LED 开关状态
  * @param  self  指向 LED 对象的指针
  * @return LED_State_t  当前状态
  */
LED_State_t LED_GetState(const LED_t *self)
{
    if (self == NULL) {
        return LED_STATE_OFF;
    }
    return (LED_State_t)self->state;
}

/**
  * @brief  获取当前 LED 亮度值
  * @param  self  指向 LED 对象的指针
  * @return uint8_t  当前亮度值 (0~100)
  */
uint8_t LED_GetBrightness(const LED_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->brightness;
}
