/**
  ******************************************************************************
  * @file    Buzzer.h
  * @brief   蜂鸣器模块子类 — 继承 ModuleBase，支持有源/无源蜂鸣器
  *
  * @details
  * 本文件定义了 Buzzer 模块的结构体和公有接口，继承自 ModuleBase 基类。
  * 基类提供 port 和 pin 属性（有源蜂鸣器使用 GPIO 通断控制），
  * 子类实现以下功能：
  *
  *   - 有源蜂鸣器 (BUZZER_TYPE_ACTIVE)：
  *       内部自带振荡源，只需 GPIO 高/低电平即可驱动发声。
  *       支持开关控制和多种脉冲鸣叫模式。
  *
  *   - 无源蜂鸣器 (BUZZER_TYPE_PASSIVE)：
  *       需要外部 PWM 信号驱动，频率决定音调。
  *       支持频率控制、音量（占空比）调节、音乐播放。
  *       提供可扩展的预留接口 Buzzer_PassiveControl_t，允许用户
  *       注入自定义的 PWM 控制函数（如配置定时器通道）。
  *
  * === 使用示例（有源蜂鸣器）===
  *
  * Buzzer_t buzzer;
  * Buzzer_Constructor(&buzzer, GPIOD, GPIO_PIN_0, BUZZER_TYPE_ACTIVE, 1);
  * ModuleBase_Init((ModuleBase_t *)&buzzer);
  *
  * Buzzer_Beep(&buzzer, 200);               // 短鸣 200ms
  * Buzzer_BeepDouble(&buzzer, 100, 50);     // 双连鸣
  * Buzzer_BeepPattern(&buzzer, melody, 8);  // 播放旋律
  *
  * === 使用示例（无源蜂鸣器）===
  *
  * // 1. 自定义 PWM 控制函数
  * static void my_pwm_control(void *self, uint16_t freq, uint8_t duty) {
  *     Buzzer_t *bz = (Buzzer_t *)self;
  *     // 配置定时器频率 = freq，占空比 = duty%
  *     __HAL_TIM_SET_AUTORELOAD(&htim, (TIM_FREQ / freq) - 1);
  *     __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, duty * ... / 100);
  * }
  *
  * // 2. 构造并注入控制函数
  * Buzzer_t buzzer;
  * Buzzer_Constructor(&buzzer, GPIOD, GPIO_PIN_0, BUZZER_TYPE_PASSIVE, 1);
  * Buzzer_SetPassiveControl(&buzzer, my_pwm_control);
  * ModuleBase_Init((ModuleBase_t *)&buzzer);
  *
  * // 3. 播放音符
  * Buzzer_PlayNote(&buzzer, NOTE_C4, 400);  // 播放 C4 音 400ms
  *
  ******************************************************************************
  */

#ifndef __BUZZER_H__
#define __BUZZER_H__

/* Includes ------------------------------------------------------------------*/
#include "ModuleBase.h"
#include <stdint.h>

/* ==================== 蜂鸣器类型枚举 ==================== */

/**
  * @brief 蜂鸣器类型
  */
typedef enum {
    BUZZER_TYPE_ACTIVE  = 0,    /**< 有源蜂鸣器（GPIO 电平驱动）*/
    BUZZER_TYPE_PASSIVE = 1     /**< 无源蜂鸣器（PWM 频率驱动）*/
} Buzzer_Type_t;

/* ==================== 蜂鸣器状态枚举 ==================== */

/**
  * @brief 蜂鸣器工作状态
  */
typedef enum {
    BUZZER_STATE_OFF       = 0, /**< 静音 */
    BUZZER_STATE_ON        = 1, /**< 持续发声 */
    BUZZER_STATE_BEEP      = 2, /**< 单次鸣叫中 */
    BUZZER_STATE_BEEP_DUAL = 3, /**< 双连鸣中 */
    BUZZER_STATE_PLAY      = 4  /**< 旋律播放中 */
} Buzzer_State_t;

/* ==================== 音符数据结构 ==================== */

/**
  * @brief 音符结构体
  * @note  用于预定义旋律数组。
  *        frequency 为 0 表示休止符（静音）。
  */
typedef struct {
    uint16_t frequency;     /**< 频率 (Hz)，0 = 休止符 */
    uint16_t duration_ms;   /**< 持续时间 (ms) */
} Buzzer_Note_t;

/* ==================== 无源蜂鸣器控制函数指针 ==================== */

/**
  * @brief 无源蜂鸣器 PWM 控制回调
  *
  * @details
  * 预留的可扩展接口。用户可注入自定义实现来配置定时器 PWM 输出。
  * 当 buzzer 类型为 BUZZER_TYPE_PASSIVE 时，内部方法通过此回调
  * 设置 PWM 频率和占空比。若未设置（为 NULL），则回退到 GPIO
  * 电平翻转实现简易发声（仅适用于低频场合）。
  *
  * @param self      指向 Buzzer_t 对象的 void 指针
  * @param frequency 期望的频率 (Hz)
  *        - 传入 0 表示关闭 PWM 输出
  * @param duty_cycle 占空比 (0~100)
  *        - 0: 完全关闭
  *        - 100: 最大占空比（满幅输出）
  */
typedef void (*Buzzer_PassiveControl_t)(void *self, uint16_t frequency, uint8_t duty_cycle);

/* ==================== Buzzer 结构体定义 ==================== */

/**
  * @brief 蜂鸣器模块结构体（继承 ModuleBase）
  *
  * @note   ModuleBase_t 必须为第一个成员，确保指针可安全转换
  * @note   port 和 pin 由基类提供；有源蜂鸣器使用 GPIO 通断，
  *         无源蜂鸣器使用 PWM（通过 passive_control 回调）
  */
typedef struct Buzzer_s {
    ModuleBase_t            base;               /**< 基类（必须为第一个成员，含 port/pin）*/
    Buzzer_Type_t           type;               /**< 蜂鸣器类型 */
    uint8_t                 active_high;        /**< 电平极性: 1=高电平响, 0=低电平响 */
    uint8_t                 volume;             /**< 音量 (0~100, 0=静音, 100=最大) */
    Buzzer_State_t          state;              /**< 当前工作状态 */

    /* 无源蜂鸣器扩展接口 */
    Buzzer_PassiveControl_t passive_control;    /**< PWM 控制回调（NULL 则回退 GPIO）*/

    /* 内部状态机（用于非阻塞鸣叫/旋律播放）*/
    uint32_t                tick_start;         /**< 当前动作起始时刻 (ms) */
    uint16_t                beep_on_ms;         /**< 鸣叫持续时间 (ms) */
    uint16_t                beep_off_ms;        /**< 间隔持续时间 (ms) */
    uint8_t                 beep_repeat;        /**< 重复次数 */
    uint8_t                 beep_count;         /**< 当前已完成的次数 */

    /* 旋律播放状态 */
    const Buzzer_Note_t    *melody;             /**< 旋律数组指针 */
    uint8_t                 melody_len;         /**< 旋律总音符数 */
    uint8_t                 melody_index;       /**< 当前播放到的音符索引 */

    /* 音量渐变状态（Run() 中非阻塞驱动）*/
    uint8_t                 ramp_volume_start;  /**< 渐变起始音量 (0~100) */
    uint8_t                 ramp_volume_target; /**< 渐变目标音量 (0~100) */
    uint32_t                ramp_tick_start;    /**< 渐变起始时刻 (ms) */
    uint16_t                ramp_duration_ms;   /**< 渐变总时长 (ms)，0=禁用 */
} Buzzer_t;

/* ==================== 预定义音符频率常量 ==================== */

/**
  * @defgroup Buzzer_NoteFrequencies 音符频率常量
  * @note   频率单位 Hz
  * @{
  */

/* 中央 C 八度 (C4 ~ B4) */
#define NOTE_C4             262U
#define NOTE_CS4            277U    /* C#4 / Db4 */
#define NOTE_D4             294U
#define NOTE_DS4            311U    /* D#4 / Eb4 */
#define NOTE_E4             330U
#define NOTE_F4             349U
#define NOTE_FS4            370U    /* F#4 / Gb4 */
#define NOTE_G4             392U
#define NOTE_GS4            415U    /* G#4 / Ab4 */
#define NOTE_A4             440U
#define NOTE_AS4            466U    /* A#4 / Bb4 */
#define NOTE_B4             494U

/* 高八度 (C5 ~ B5) */
#define NOTE_C5             523U
#define NOTE_CS5            554U    /* C#5 / Db5 */
#define NOTE_D5             587U
#define NOTE_DS5            622U    /* D#5 / Eb5 */
#define NOTE_E5             659U
#define NOTE_F5             698U
#define NOTE_FS5            740U    /* F#5 / Gb5 */
#define NOTE_G5             784U
#define NOTE_GS5            831U    /* G#5 / Ab5 */
#define NOTE_A5             880U
#define NOTE_AS5            932U    /* A#5 / Bb5 */
#define NOTE_B5             988U

/* 低八度 (C3 ~ B3) */
#define NOTE_C3             131U
#define NOTE_CS3            139U
#define NOTE_D3             147U
#define NOTE_DS3            156U
#define NOTE_E3             165U
#define NOTE_F3             175U
#define NOTE_FS3            185U
#define NOTE_G3             196U
#define NOTE_GS3            208U
#define NOTE_A3             220U
#define NOTE_AS3            233U
#define NOTE_B3             247U

/** 休止符 */
#define NOTE_REST           0U

/**
  * @}
  */

/* ==================== 预定义旋律数组 ==================== */

/**
  * @defgroup Buzzer_PredefinedMelodies 预定义旋律
  * @note   外部可通过 extern 引用这些旋律数组
  * @{
  */

/** @brief "小星星" 旋律（8 个音符 + 结束标志）*/
extern const Buzzer_Note_t melody_twinkle_twinkle[];

/** @brief "滴滴" 提示音（2 个短促高音）*/
extern const Buzzer_Note_t melody_beep_notify[];

/**
  * @}
  */

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  蜂鸣器构造函数
  * @note   初始化基类成员，设置蜂鸣器类型和电平极性。
  *         构造后默认静音，音量 100%。
  *         对于无源蜂鸣器，需额外调用 Buzzer_SetPassiveControl()
  *         设置 PWM 控制回调。
  * @param  self         指向 Buzzer 对象的指针
  * @param  port         蜂鸣器所在的 GPIO 端口
  * @param  pin          蜂鸣器所在的 GPIO 引脚
  * @param  type         蜂鸣器类型 (BUZZER_TYPE_ACTIVE / BUZZER_TYPE_PASSIVE)
  * @param  active_high  电平极性: 1 = 高电平响, 0 = 低电平响
  */
void Buzzer_Constructor(Buzzer_t *self, GPIO_TypeDef *port, uint16_t pin,
                        Buzzer_Type_t type, uint8_t active_high);

/**
  * @brief  设置无源蜂鸣器的 PWM 控制回调
  * @note   仅在 type == BUZZER_TYPE_PASSIVE 时需要调用。
  *         若不设置，无源蜂鸣器将回退到 GPIO 翻转方式（简易模式）。
  * @param  self    指向 Buzzer 对象的指针
  * @param  control PWM 控制回调函数指针
  */
void Buzzer_SetPassiveControl(Buzzer_t *self, Buzzer_PassiveControl_t control);

/**
  * @brief  开启蜂鸣器（持续发声）
  * @note   有源蜂鸣器：GPIO 输出有效电平。
  *         无源蜂鸣器：调用 passive_control 启动 PWM。
  * @param  self  指向 Buzzer 对象的指针
  */
void Buzzer_On(Buzzer_t *self);

/**
  * @brief  关闭蜂鸣器（静音）
  * @param  self  指向 Buzzer 对象的指针
  */
void Buzzer_Off(Buzzer_t *self);

/**
  * @brief  单次鸣叫
  * @note   阻塞方式鸣叫指定的时长后自动关闭。
  * @param  self        指向 Buzzer 对象的指针
  * @param  duration_ms 鸣叫时长 (ms)
  */
void Buzzer_Beep(Buzzer_t *self, uint16_t duration_ms);

/**
  * @brief  双连鸣（类似提示音 "哔哔"）
  * @note   阻塞方式：响 -> 停 -> 响。
  * @param  self      指向 Buzzer 对象的指针
  * @param  on_ms     每次鸣叫时长 (ms)
  * @param  off_ms    间隔时长 (ms)
  */
void Buzzer_BeepDouble(Buzzer_t *self, uint16_t on_ms, uint16_t off_ms);

/**
  * @brief  播放预定义模式的脉冲序列
  * @note   阻塞方式播放 repeat 次（on_ms 响 + off_ms 停）的脉冲序列。
  *         适用于自定义报警模式，如 SOS（短长短）。
  * @param  self      指向 Buzzer 对象的指针
  * @param  on_ms     每次鸣叫时长 (ms)
  * @param  off_ms    间隔时长 (ms)
  * @param  repeat    重复次数
  */
void Buzzer_BeepPattern(Buzzer_t *self, uint16_t on_ms, uint16_t off_ms, uint8_t repeat);

/**
  * @brief  播放一个音符（无源蜂鸣器）
  * @note   仅在 type == BUZZER_TYPE_PASSIVE 时有效。
  *         阻塞方式以指定频率发声 duration_ms 后关闭。
  *         若 frequency == 0（休止符），则仅延时静音。
  * @param  self         指向 Buzzer 对象的指针
  * @param  frequency    音符频率 (Hz)，0 = 休止符
  * @param  duration_ms  持续时间 (ms)
  */
void Buzzer_PlayNote(Buzzer_t *self, uint16_t frequency, uint16_t duration_ms);

/**
  * @brief  播放旋律数组（无源蜂鸣器）
  * @note   仅在 type == BUZZER_TYPE_PASSIVE 时有效。
  *         阻塞方式依次播放旋律数组中的每个音符。
  * @param  self    指向 Buzzer 对象的指针
  * @param  melody  指向音符数组的指针（以 NOTE_REST + duration=0 结束）
  * @param  len     音符数量
  */
void Buzzer_PlayMelody(Buzzer_t *self, const Buzzer_Note_t *melody, uint8_t len);

/**
  * @brief  设置音量
  * @note   有源蜂鸣器：音量仅控制是否发声（volume > 0 即可）。
  *         无源蜂鸣器：音量控制 PWM 占空比（需 passive_control 支持）。
  * @param  self    指向 Buzzer 对象的指针
  * @param  volume  音量 (0~100, 0=静音, 100=最大)
  */
void Buzzer_SetVolume(Buzzer_t *self, uint8_t volume);

/**
  * @brief  平滑渐变音量（音量缓变）
  * @note   非阻塞状态机触发的音量渐变，由 Run() 中的状态机驱动。
  *         调用此函数设置目标音量和渐变时长，Run() 中逐步调整。
  *
  *         若需要阻塞方式渐变，可循环调用 Buzzer_SetVolume() + HAL_Delay()。
  *
  * @param  self          指向 Buzzer 对象的指针
  * @param  target_volume 目标音量 (0~100)
  * @param  ramp_duration_ms 渐变总时长 (ms)
  *         - 0: 立即跳变到目标音量
  */
void Buzzer_RampVolume(Buzzer_t *self, uint8_t target_volume, uint16_t ramp_duration_ms);

/**
  * @brief  获取当前蜂鸣器状态
  * @param  self  指向 Buzzer 对象的指针
  * @return Buzzer_State_t  当前工作状态
  */
Buzzer_State_t Buzzer_GetState(const Buzzer_t *self);

/**
  * @brief  获取当前音量
  * @param  self  指向 Buzzer 对象的指针
  * @return uint8_t  当前音量 (0~100)
  */
uint8_t Buzzer_GetVolume(const Buzzer_t *self);

#endif /* __BUZZER_H__ */
