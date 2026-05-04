/**
  ******************************************************************************
  * @file    Buzzer.c
  * @brief   蜂鸣器模块子类实现 — 有源/无源蜂鸣器控制
  *
  * @details
  * 提供 Buzzer_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - 有源蜂鸣器 GPIO 通断控制
  *   - 无源蜂鸣器 PWM 回调扩展接口
  *   - 多种脉冲鸣叫模式（单鸣、双鸣、自定义模式）
  *   - 音量控制与平滑渐变
  *   - 预定义音符频率与旋律数组
  *
  * === 音量渐变设计说明 ===
  *
  * Buzzer_RampVolume() 设置渐变参数（起始音量、目标音量、时长），
  * Run() 中每 50ms 采样一次，按线性插值逐步调整音量。
  * 渐变完成后自动禁用渐变状态。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Buzzer.h"
#include "stm32f4xx_hal.h"      /* HAL_GPIO_WritePin, HAL_GetTick, HAL_Delay */

/* ==================== 私有宏定义 ==================== */

/**
  * @defgroup Buzzer_Private_Macros 私有宏
  * @{
  */
#define BUZZER_VOLUME_MAX           100U    /**< 音量最大值 */
#define BUZZER_VOLUME_MIN           0U      /**< 音量最小值 */
#define BUZZER_RAMP_SAMPLE_MS       50U     /**< 音量渐变采样间隔 (ms) */
#define BUZZER_DEFAULT_FREQ_HZ      2000U   /**< 无源蜂鸣器默认频率 (Hz) */
/**
  * @}
  */

/* ==================== 私有辅助函数（前向声明） ==================== */

static void Buzzer_WritePin(Buzzer_t *self, uint8_t on);
static void Buzzer_SetOutput(Buzzer_t *self, uint8_t on, uint16_t frequency);
static void Buzzer_ApplyVolume(Buzzer_t *self);

/* ==================== 私有辅助函数实现 ==================== */

/**
  * @brief  根据电平极性写入 GPIO
  * @note   封装了 active_high 逻辑。
  *         有源蜂鸣器直接使用 GPIO 通断控制。
  * @param  self  指向 Buzzer 对象的指针（非空）
  * @param  on    1 = 响, 0 = 静音
  */
static void Buzzer_WritePin(Buzzer_t *self, uint8_t on)
{
    GPIO_PinState pin_state;

    /* 根据电平极性决定 GPIO 输出电平 */
    if (self->active_high) {
        pin_state = (on) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        pin_state = (on) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    /* port 和 pin 继承自基类 ModuleBase_t */
    HAL_GPIO_WritePin(self->base.port, self->base.pin, pin_state);
}

/**
  * @brief  设置蜂鸣器输出
  * @note   根据类型选择输出方式：
  *         - 有源：GPIO 通断
  *         - 无源 + 有回调：通过 passive_control 设置 PWM
  *         - 无源 + 无回调：回退到 GPIO 通断
  * @param  self       指向 Buzzer 对象的指针
  * @param  on         1 = 响, 0 = 静音
  * @param  frequency  频率 (Hz)，仅无源蜂鸣器有效
  */
static void Buzzer_SetOutput(Buzzer_t *self, uint8_t on, uint16_t frequency)
{
    if (self->type == BUZZER_TYPE_ACTIVE) {
        /* 有源蜂鸣器：直接 GPIO 通断 */
        Buzzer_WritePin(self, on);
    } else {
        /* 无源蜂鸣器 */
        if (self->passive_control != NULL) {
            /* 通过回调设置 PWM（用户自定义定时器配置）*/
            uint8_t duty = on ? self->volume : 0;
            self->passive_control((void *)self, on ? frequency : 0, duty);
        } else {
            /* 未设置 PWM 回调，回退到 GPIO 通断 */
            Buzzer_WritePin(self, on);
        }
    }
}

/**
  * @brief  将当前音量应用到无源蜂鸣器的 PWM 占空比
  * @note   仅在 passive_control 已设置时有效。
  */
static void Buzzer_ApplyVolume(Buzzer_t *self)
{
    if ((self->type == BUZZER_TYPE_PASSIVE) &&
        (self->passive_control != NULL) &&
        (self->state != BUZZER_STATE_OFF)) {
        /* 保持当前频率，仅更新占空比 */
        self->passive_control((void *)self, BUZZER_DEFAULT_FREQ_HZ, self->volume);
    }
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  蜂鸣器初始化虚函数
  * @note   验证参数有效性，初始状态为静音。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    参数无效
  */
static int Buzzer_init(void *self)
{
    Buzzer_t *bz = (Buzzer_t *)self;

    /* 参数完整性检查 */
    if ((bz->base.port == NULL) || (bz->base.pin == 0)) {
        return -1;
    }

    /* 初始状态：静音 */
    Buzzer_SetOutput(bz, 0, 0);
    bz->state       = BUZZER_STATE_OFF;
    bz->volume      = BUZZER_VOLUME_MAX;
    bz->tick_start  = 0;

    /* 清空状态机参数 */
    bz->beep_on_ms    = 0;
    bz->beep_off_ms   = 0;
    bz->beep_repeat   = 0;
    bz->beep_count    = 0;
    bz->melody        = NULL;
    bz->melody_len    = 0;
    bz->melody_index  = 0;

    /* 禁用音量渐变 */
    bz->ramp_duration_ms   = 0;
    bz->ramp_volume_start  = BUZZER_VOLUME_MAX;
    bz->ramp_volume_target = BUZZER_VOLUME_MAX;
    bz->ramp_tick_start    = 0;

    return 0;
}

/**
  * @brief  蜂鸣器运行虚函数 — 非阻塞音量渐变驱动
  * @note   在 main 循环中周期性调用，驱动音量渐变状态机。
  *         渐变通过线性插值每 BUZZER_RAMP_SAMPLE_MS 调整一次音量。
  *
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int Buzzer_run(void *self)
{
    Buzzer_t *bz = (Buzzer_t *)self;

    /* === 音量渐变状态机 === */
    if (bz->ramp_duration_ms > 0) {
        uint32_t now       = HAL_GetTick();
        uint32_t elapsed   = now - bz->ramp_tick_start;

        if (elapsed >= bz->ramp_duration_ms) {
            /* 渐变完成：设定为目标音量并禁用渐变 */
            bz->volume            = bz->ramp_volume_target;
            bz->ramp_duration_ms  = 0;
            Buzzer_ApplyVolume(bz);
        } else {
            /* 渐变中：线性插值 */
            uint32_t progress = (elapsed * BUZZER_VOLUME_MAX) / bz->ramp_duration_ms;
            int16_t delta     = (int16_t)bz->ramp_volume_target - (int16_t)bz->ramp_volume_start;
            int16_t new_vol   = (int16_t)bz->ramp_volume_start +
                                (int16_t)((int32_t)delta * (int32_t)elapsed / (int32_t)bz->ramp_duration_ms);

            /* 限幅 */
            if (new_vol < 0)           new_vol = 0;
            if (new_vol > BUZZER_VOLUME_MAX) new_vol = BUZZER_VOLUME_MAX;

            bz->volume = (uint8_t)new_vol;
            Buzzer_ApplyVolume(bz);

            (void)progress; /* 仅用于计算可读性 */
        }
    }

    return 0;
}

/**
  * @brief  蜂鸣器清理虚函数
  * @note   静音蜂鸣器，重置所有状态。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int Buzzer_cleanup(void *self)
{
    Buzzer_t *bz = (Buzzer_t *)self;

    /* 静音 */
    Buzzer_SetOutput(bz, 0, 0);
    bz->state  = BUZZER_STATE_OFF;
    bz->volume = BUZZER_VOLUME_MAX;

    /* 重置状态机 */
    bz->tick_start  = 0;
    bz->beep_on_ms  = 0;
    bz->beep_off_ms = 0;
    bz->beep_repeat = 0;
    bz->beep_count  = 0;
    bz->melody      = NULL;
    bz->melody_len  = 0;
    bz->melody_index = 0;

    /* 禁用渐变 */
    bz->ramp_duration_ms = 0;

    return 0;
}

/**
  * @brief  蜂鸣器复位虚函数
  * @note   复位到初始状态：静音，音量 100%。
  * @param  self  指向模块对象自身的 void 指针
  */
static void Buzzer_reset(void *self)
{
    Buzzer_t *bz = (Buzzer_t *)self;

    /* 静音 */
    Buzzer_SetOutput(bz, 0, 0);
    bz->state  = BUZZER_STATE_OFF;
    bz->volume = BUZZER_VOLUME_MAX;

    /* 重置状态机 */
    bz->tick_start   = 0;
    bz->beep_on_ms   = 0;
    bz->beep_off_ms  = 0;
    bz->beep_repeat  = 0;
    bz->beep_count   = 0;
    bz->melody       = NULL;
    bz->melody_len   = 0;
    bz->melody_index = 0;

    /* 禁用渐变 */
    bz->ramp_duration_ms = 0;
}

/* ==================== 子类虚函数表实例 ==================== */

/**
  * @brief 蜂鸣器模块虚函数表
  * @note  所有虚函数均被重写，提供完整的蜂鸣器控制行为。
  */
static const ModuleVTable_t buzzer_vtable = {
    .init    = Buzzer_init,
    .run     = Buzzer_run,
    .cleanup = Buzzer_cleanup,
    .reset   = Buzzer_reset,
};

/* ==================== 预定义旋律数组 ==================== */

/**
  * @brief "小星星" 旋律
  * @note   C4 C4 G4 G4 A4 A4 G4 (休止) F4 F4 E4 E4 D4 D4 C4
  *         每个音符持续 400ms。
  */
const Buzzer_Note_t melody_twinkle_twinkle[] = {
    {NOTE_C4, 400}, {NOTE_C4, 400}, {NOTE_G4, 400}, {NOTE_G4, 400},
    {NOTE_A4, 400}, {NOTE_A4, 400}, {NOTE_G4, 800}, {NOTE_REST, 100},
    {NOTE_F4, 400}, {NOTE_F4, 400}, {NOTE_E4, 400}, {NOTE_E4, 400},
    {NOTE_D4, 400}, {NOTE_D4, 400}, {NOTE_C4, 800},
    {NOTE_REST, 0}  /* 结束标志 */
};

/**
  * @brief "滴滴" 提示音
  * @note   两个短促高音 (C5)，类似设备提示音。
  */
const Buzzer_Note_t melody_beep_notify[] = {
    {NOTE_C5, 150}, {NOTE_REST, 100}, {NOTE_C5, 150},
    {NOTE_REST, 0}  /* 结束标志 */
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  蜂鸣器构造函数
  * @param  self         指向 Buzzer 对象的指针
  * @param  port         蜂鸣器所在的 GPIO 端口
  * @param  pin          蜂鸣器所在的 GPIO 引脚
  * @param  type         蜂鸣器类型
  * @param  active_high  电平极性: 1 = 高电平响, 0 = 低电平响
  */
void Buzzer_Constructor(Buzzer_t *self, GPIO_TypeDef *port, uint16_t pin,
                        Buzzer_Type_t type, uint8_t active_high)
{
    if (self == NULL) {
        return;
    }

    /* 1. 调用基类构造函数 */
    ModuleBase_Constructor(&self->base, "BUZZER");

    /* 2. 设置 GPIO 端口和引脚到基类属性中 */
    ModuleBase_SetPinPort(&self->base, port, pin);

    /* 3. 初始化蜂鸣器子类特有成员 */
    self->type            = type;
    self->active_high     = active_high;
    self->volume          = BUZZER_VOLUME_MAX;
    self->state           = BUZZER_STATE_OFF;
    self->passive_control = NULL;

    /* 清空状态机参数 */
    self->tick_start   = 0;
    self->beep_on_ms   = 0;
    self->beep_off_ms  = 0;
    self->beep_repeat  = 0;
    self->beep_count   = 0;
    self->melody       = NULL;
    self->melody_len   = 0;
    self->melody_index = 0;

    /* 禁用音量渐变 */
    self->ramp_duration_ms   = 0;
    self->ramp_volume_start  = BUZZER_VOLUME_MAX;
    self->ramp_volume_target = BUZZER_VOLUME_MAX;
    self->ramp_tick_start    = 0;

    /* 4. 替换为子类虚函数表 */
    self->base.vtable = &buzzer_vtable;
}

/**
  * @brief  设置无源蜂鸣器的 PWM 控制回调
  * @param  self    指向 Buzzer 对象的指针
  * @param  control PWM 控制回调函数指针
  */
void Buzzer_SetPassiveControl(Buzzer_t *self, Buzzer_PassiveControl_t control)
{
    if (self == NULL) {
        return;
    }

    self->passive_control = control;
}

/**
  * @brief  开启蜂鸣器（持续发声）
  * @param  self  指向 Buzzer 对象的指针
  */
void Buzzer_On(Buzzer_t *self)
{
    if (self == NULL) {
        return;
    }

    self->state = BUZZER_STATE_ON;
    Buzzer_SetOutput(self, 1, BUZZER_DEFAULT_FREQ_HZ);
}

/**
  * @brief  关闭蜂鸣器（静音）
  * @param  self  指向 Buzzer 对象的指针
  */
void Buzzer_Off(Buzzer_t *self)
{
    if (self == NULL) {
        return;
    }

    self->state = BUZZER_STATE_OFF;
    Buzzer_SetOutput(self, 0, 0);
}

/**
  * @brief  单次鸣叫（阻塞方式）
  * @param  self        指向 Buzzer 对象的指针
  * @param  duration_ms 鸣叫时长 (ms)
  */
void Buzzer_Beep(Buzzer_t *self, uint16_t duration_ms)
{
    if (self == NULL) {
        return;
    }

    self->state = BUZZER_STATE_BEEP;

    /* 开启 */
    Buzzer_SetOutput(self, 1, BUZZER_DEFAULT_FREQ_HZ);

    /* 延时 */
    HAL_Delay(duration_ms);

    /* 关闭 */
    Buzzer_SetOutput(self, 0, 0);
    self->state = BUZZER_STATE_OFF;
}

/**
  * @brief  双连鸣（阻塞方式）
  * @param  self      指向 Buzzer 对象的指针
  * @param  on_ms     每次鸣叫时长 (ms)
  * @param  off_ms    间隔时长 (ms)
  */
void Buzzer_BeepDouble(Buzzer_t *self, uint16_t on_ms, uint16_t off_ms)
{
    if (self == NULL) {
        return;
    }

    self->state = BUZZER_STATE_BEEP_DUAL;

    /* 第一次鸣叫 */
    Buzzer_SetOutput(self, 1, BUZZER_DEFAULT_FREQ_HZ);
    HAL_Delay(on_ms);
    Buzzer_SetOutput(self, 0, 0);

    /* 间隔 */
    HAL_Delay(off_ms);

    /* 第二次鸣叫 */
    Buzzer_SetOutput(self, 1, BUZZER_DEFAULT_FREQ_HZ);
    HAL_Delay(on_ms);
    Buzzer_SetOutput(self, 0, 0);

    self->state = BUZZER_STATE_OFF;
}

/**
  * @brief  播放脉冲序列（阻塞方式）
  * @param  self      指向 Buzzer 对象的指针
  * @param  on_ms     每次鸣叫时长 (ms)
  * @param  off_ms    间隔时长 (ms)
  * @param  repeat    重复次数
  */
void Buzzer_BeepPattern(Buzzer_t *self, uint16_t on_ms, uint16_t off_ms, uint8_t repeat)
{
    uint8_t i;

    if ((self == NULL) || (repeat == 0)) {
        return;
    }

    self->state = BUZZER_STATE_BEEP;

    for (i = 0; i < repeat; i++) {
        /* 鸣叫 */
        Buzzer_SetOutput(self, 1, BUZZER_DEFAULT_FREQ_HZ);
        HAL_Delay(on_ms);
        Buzzer_SetOutput(self, 0, 0);

        /* 间隔（最后一次不等待）*/
        if (i < repeat - 1) {
            HAL_Delay(off_ms);
        }
    }

    self->state = BUZZER_STATE_OFF;
}

/**
  * @brief  播放一个音符（无源蜂鸣器，阻塞方式）
  * @param  self         指向 Buzzer 对象的指针
  * @param  frequency    音符频率 (Hz)，0 = 休止符
  * @param  duration_ms  持续时间 (ms)
  */
void Buzzer_PlayNote(Buzzer_t *self, uint16_t frequency, uint16_t duration_ms)
{
    if (self == NULL) {
        return;
    }

    /* 仅无源蜂鸣器支持音符播放 */
    if (self->type != BUZZER_TYPE_PASSIVE) {
        return;
    }

    self->state = BUZZER_STATE_PLAY;

    if (frequency > 0) {
        /* 发声 */
        Buzzer_SetOutput(self, 1, frequency);
    }
    /* 若 frequency == 0（休止符），仅延时静音 */

    HAL_Delay(duration_ms);

    /* 关闭 */
    Buzzer_SetOutput(self, 0, 0);
    self->state = BUZZER_STATE_OFF;
}

/**
  * @brief  播放旋律数组（无源蜂鸣器，阻塞方式）
  * @param  self    指向 Buzzer 对象的指针
  * @param  melody  指向音符数组的指针
  * @param  len     音符数量
  */
void Buzzer_PlayMelody(Buzzer_t *self, const Buzzer_Note_t *melody, uint8_t len)
{
    uint8_t i;

    if ((self == NULL) || (melody == NULL) || (len == 0)) {
        return;
    }

    /* 仅无源蜂鸣器支持旋律播放 */
    if (self->type != BUZZER_TYPE_PASSIVE) {
        return;
    }

    self->state = BUZZER_STATE_PLAY;

    for (i = 0; i < len; i++) {
        /* 遇到结束标志（NOTE_REST + duration=0）提前退出 */
        if ((melody[i].frequency == NOTE_REST) && (melody[i].duration_ms == 0)) {
            break;
        }

        if (melody[i].frequency > 0) {
            Buzzer_SetOutput(self, 1, melody[i].frequency);
        } else {
            Buzzer_SetOutput(self, 0, 0);
        }

        HAL_Delay(melody[i].duration_ms);

        /* 关闭 */
        Buzzer_SetOutput(self, 0, 0);
    }

    self->state = BUZZER_STATE_OFF;
}

/**
  * @brief  设置音量
  * @param  self    指向 Buzzer 对象的指针
  * @param  volume  音量 (0~100, 0=静音, 100=最大)
  */
void Buzzer_SetVolume(Buzzer_t *self, uint8_t volume)
{
    if (self == NULL) {
        return;
    }

    /* 限幅 */
    if (volume > BUZZER_VOLUME_MAX) {
        volume = BUZZER_VOLUME_MAX;
    }

    self->volume = volume;

    /* 有源蜂鸣器：音量 > 0 时维持当前状态 */
    if (self->type == BUZZER_TYPE_ACTIVE) {
        if (self->state != BUZZER_STATE_OFF) {
            if (volume == 0) {
                Buzzer_WritePin(self, 0);
            } else {
                Buzzer_WritePin(self, 1);
            }
        }
        return;
    }

    /* 无源蜂鸣器：更新 PWM 占空比 */
    if (self->state != BUZZER_STATE_OFF) {
        Buzzer_ApplyVolume(self);
    }
}

/**
  * @brief  平滑渐变音量（由 Run() 中的状态机非阻塞驱动）
  * @param  self              指向 Buzzer 对象的指针
  * @param  target_volume     目标音量 (0~100)
  * @param  ramp_duration_ms  渐变总时长 (ms)，0=立即跳变
  */
void Buzzer_RampVolume(Buzzer_t *self, uint8_t target_volume, uint16_t ramp_duration_ms)
{
    if (self == NULL) {
        return;
    }

    /* 限幅 */
    if (target_volume > BUZZER_VOLUME_MAX) {
        target_volume = BUZZER_VOLUME_MAX;
    }

    if (ramp_duration_ms == 0) {
        /* 立即跳变 */
        Buzzer_SetVolume(self, target_volume);
        return;
    }

    /* 设置渐变参数，由 Run() 中的状态机驱动 */
    self->ramp_volume_start  = self->volume;
    self->ramp_volume_target = target_volume;
    self->ramp_tick_start    = HAL_GetTick();
    self->ramp_duration_ms   = ramp_duration_ms;
}

/**
  * @brief  获取当前蜂鸣器状态
  * @param  self  指向 Buzzer 对象的指针
  * @return Buzzer_State_t  当前工作状态
  */
Buzzer_State_t Buzzer_GetState(const Buzzer_t *self)
{
    if (self == NULL) {
        return BUZZER_STATE_OFF;
    }
    return self->state;
}

/**
  * @brief  获取当前音量
  * @param  self  指向 Buzzer 对象的指针
  * @return uint8_t  当前音量 (0~100)
  */
uint8_t Buzzer_GetVolume(const Buzzer_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->volume;
}
