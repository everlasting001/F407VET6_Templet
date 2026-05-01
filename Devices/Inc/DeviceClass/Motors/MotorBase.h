/**
  ******************************************************************************
  * @file    MotorBase.h
  * @brief   电机模块基类 — 继承 ModuleBase 的通用电机抽象接口
  *
  * @details
  * 本文件定义了所有电机类型（DC、步进、舵机）的基类接口。
  * 继承自 ModuleBase 基类，在此基础上扩展电机专用的属性与方法：
  *   - 多引脚配置（使能、方向、PWM 等）
  *   - 旋转方向控制（含极性反转）
  *   - 电机类型与状态管理
  *   - 便携式 HAL 抽象层（可替换底层硬件操作）
  *
  * === 典型用法（DC 电机） ===
  *
  * // 1. 定义 GPIO 引脚配置数组
  * static const GPIO_PinConfig dc_pins[] = {
  *     {GPIOC, GPIO_PIN_0},  // 使能
  *     {GPIOC, GPIO_PIN_1},  // 方向
  *     {GPIOC, GPIO_PIN_2},  // PWM
  * };
  *
  * // 2. 定义并构造电机对象
  * MotorBase motor;
  * Motor_Constructor(&motor, "DC_Motor", MOTOR_TYPE_DC, dc_pins, 3);
  *
  * // 3. 初始化
  * Motor_Init(&motor);
  *
  * // 4. 控制
  * Motor_SetDirection(&motor, MOTOR_DIR_CW);
  * Motor_Stop(&motor);
  *
  * // 5. 获取状态
  * MotorStatus status;
  * Motor_GetStatus(&motor, &status);
  *
  ******************************************************************************
  */

#ifndef __MOTOR_BASE_H__
#define __MOTOR_BASE_H__

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"
#include <stdint.h>

/* ==================== 版本与常量定义 ==================== */

/** @brief 电机基类结构体版本号，便于兼容性检查 */
#define MOTOR_BASE_VERSION   0x0001U

/** @brief 单个电机支持的最大引脚数量 */
#define MOTOR_PIN_MAX        8U

/** @brief 极性反转关闭 */
#define MOTOR_POLARITY_NORMAL  0U
/** @brief 极性反转开启（硬件接线反向时使用） */
#define MOTOR_POLARITY_INVERT  1U

/* ==================== 错误码枚举 ==================== */

/**
  * @brief 电机操作错误码
  * @note  0 表示成功，负值表示各类错误
  */
typedef enum {
    MOTOR_OK            =  0,   /**< 操作成功 */
    MOTOR_ERR_NULL      = -1,   /**< 空指针错误 */
    MOTOR_ERR_CONFIG    = -2,   /**< 配置错误（引脚无效、参数越界） */
    MOTOR_ERR_BUSY      = -3,   /**< 电机忙（正在执行操作） */
    MOTOR_ERR_STATE     = -4,   /**< 状态错误（当前状态下不允许该操作） */
    MOTOR_ERR_PARAM     = -5,   /**< 参数无效 */
    MOTOR_ERR_TIMEOUT   = -6,   /**< 操作超时 */
    MOTOR_ERR_HARDWARE  = -7    /**< 硬件错误 */
} MotorError;

/* ==================== 方向枚举 ==================== */

/**
  * @brief 电机旋转方向
  * @note  配合极性反转标志，自动处理硬件接线差异
  */
typedef enum {
    MOTOR_DIR_CW  = 0,          /**< 顺时针（正向） */
    MOTOR_DIR_CCW = 1           /**< 逆时针（反向） */
} MotorDirection;

/* ==================== 电机类型枚举 ==================== */

/**
  * @brief 电机类型
  */
typedef enum {
    MOTOR_TYPE_DC      = 0,     /**< DC 有刷电机 */
    MOTOR_TYPE_STEPPER = 1,     /**< 步进电机 */
    MOTOR_TYPE_SERVO   = 2      /**< 舵机 */
} MotorType;

/* ==================== 电机状态枚举 ==================== */

/**
  * @brief 电机运行状态
  */
typedef enum {
    MOTOR_STATE_STOPPED = 0,    /**< 已停止 */
    MOTOR_STATE_RUNNING = 1,    /**< 运行中 */
    MOTOR_STATE_ERROR   = 2     /**< 错误状态 */
} MotorState;

/* ==================== 虚函数表定义 ==================== */

/**
  * @brief 电机模块虚函数表
  * @note  子类通过提供自定义实现来重写这些虚函数。
  *        若某个函数指针为 NULL，则调用时使用默认空实现。
  * @param self  指向电机对象自身的 void 指针
  * @return int  0 = 成功, 负数 = 错误码
  */
typedef struct {
    int (*init)(void *self);        /**< 初始化电机硬件 */
    int (*run)(void *self);         /**< 电机主循环/状态机刷新 */
    int (*cleanup)(void *self);     /**< 清理电机资源 */
    void (*reset)(void *self);      /**< 复位电机到初始状态 */
} MotorVTable;

/* ==================== GPIO 引脚配置结构体 ==================== */

/**
  * @brief GPIO 引脚配置（端口 + 引脚号）
  * @note  用于构造电机对象时传入的多引脚数组
  */
typedef struct {
    GPIO_TypeDef *Port;         /**< GPIO 端口（如 GPIOA, GPIOB, GPIOC） */
    uint16_t      Pin;          /**< GPIO 引脚号（如 GPIO_PIN_0） */
} GPIO_PinConfig;

/* ==================== 电机状态查询结构体 ==================== */

/**
  * @brief 电机状态快照（供 Motor_GetStatus 填充）
  */
typedef struct {
    MotorState      state;           /**< 当前运行状态 */
    MotorDirection  direction;       /**< 当前旋转方向 */
    int32_t         position;        /**< 当前位置（编码器计数值或步数） */
    uint32_t        speed;           /**< 当前速度（PWM 占空比或步/秒） */
    uint8_t         limit_flags;     /**< 限位标志 (bit0=正向限位, bit1=反向限位) */
    uint8_t         is_initialized;  /**< 是否已初始化 */
} MotorStatus;

/* ==================== 硬件抽象层（HAL 回调） ==================== */

/**
  * @brief 硬件抽象层函数表
  * @note   封装底层 GPIO/PWM/延时操作，便于跨平台移植。
  *         默认实现使用 STM32 HAL 库，子类或应用层可替换。
  */
typedef struct {
    /**
      * @brief  写 GPIO 引脚电平
      * @param  port   GPIO 端口
      * @param  pin    GPIO 引脚号
      * @param  state  0 = 低电平, 非 0 = 高电平
      * @retval 0      成功
      * @retval 负数   错误码
      */
    int (*gpio_write)(GPIO_TypeDef *port, uint16_t pin, uint8_t state);

    /**
      * @brief  启动 PWM 输出
      * @param  timer    定时器句柄指针
      * @param  channel  定时器通道
      * @retval 0        成功
      * @retval 负数     错误码
      */
    int (*pwm_start)(void *timer, uint32_t channel);

    /**
      * @brief  停止 PWM 输出
      * @param  timer    定时器句柄指针
      * @param  channel  定时器通道
      * @retval 0        成功
      * @retval 负数     错误码
      */
    int (*pwm_stop)(void *timer, uint32_t channel);

    /**
      * @brief  设置 PWM 占空比
      * @param  timer    定时器句柄指针
      * @param  channel  定时器通道
      * @param  duty     占空比值（分辨率由子类定义，如 0~999）
      * @retval 0        成功
      * @retval 负数     错误码
      */
    int (*pwm_set_duty)(void *timer, uint32_t channel, uint32_t duty);

    /**
      * @brief  微秒级延时
      * @param  us  延时时长（微秒）
      */
    void (*delay_us)(uint32_t us);

    /**
      * @brief  获取系统滴答计数
      * @return uint32_t  当前系统滴答（毫秒）
      */
    uint32_t (*get_tick)(void);
} MotorHAL;

/* ==================== 电机基类结构体 ==================== */

/**
  * @brief 电机基类结构体（继承 ModuleBase）
  *
  * @note   ModuleBase_t 必须为第一个成员，确保指针可安全转换为基类指针。
  *         通过 ModuleBase 的虚函数表机制，子类可实现多态行为。
  *
  * @note   引脚使用 GPIO_PinConfig 数组描述，通过索引访问不同功能的引脚。
  *         索引含义由子类约定（如 [0]=使能, [1]=方向, [2]=PWM 等）。
  *
  * @note   version/checksum 字段用于结构体版本校验与内存完整性检查。
  */
typedef struct MotorBase_s {
    const MotorVTable *vtable;               /**< 虚函数表指针（指向子类或默认 vtable） */
    const char        *name;                 /**< 电机名称（调试标识） */
    uint8_t            initialized;          /**< 初始化标志 (0=未初始化, 1=已初始化) */
    GPIO_TypeDef      *port;                 /**< 主 GPIO 端口（简单单引脚电机用） */
    uint16_t           pin;                  /**< 主 GPIO 引脚（简单单引脚电机用） */
    GPIO_PinConfig     pins[MOTOR_PIN_MAX];  /**< GPIO 引脚配置数组 */
    uint8_t          pin_count;               /**< 实际使用的引脚数量 */
    MotorDirection   direction;               /**< 当前旋转方向 */
    uint8_t          dir_polarity_invert;     /**< 极性反转标志（0=正常, 1=反转） */
    MotorType        type;                    /**< 电机类型 */
    MotorState       state;                   /**< 当前运行状态 */
    uint32_t         speed;                   /**< 当前速度（单位由子类定义） */
    int32_t          position;                /**< 当前位置（编码器计数/步数） */
    int32_t          target_position;         /**< 目标位置 */
    uint8_t          limit_flags;             /**< 限位标志 (bit0=+, bit1=-) */
    uint16_t         version;                 /**< 结构体版本号 */
    uint16_t         checksum;               /**< 结构体校验和 */
    const MotorHAL  *hal;                     /**< 硬件抽象层函数表指针 */
    /* ---- 以下为私有成员，子类可扩展 ---- */
    void            *timer_handle;            /**< 定时器句柄（PWM 用） */
    uint32_t         pwm_channel;            /**< PWM 通道 */
} MotorBase;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  电机构造函数
  * @note   初始化基类成员，拷贝引脚配置，设置默认方向与停止状态。
  *         必须在任何电机操作之前调用。
  * @param  self      指向电机基类对象的指针
  * @param  name      电机名称（仅保存指针，需在生命周期内有效）
  * @param  type      电机类型
  * @param  pins      引脚配置数组指针
  * @param  pin_count 引脚数量（不超过 MOTOR_PIN_MAX）
  */
void Motor_Constructor(MotorBase *self, const char *name, MotorType type,
                       const GPIO_PinConfig *pins, uint8_t pin_count);

/**
  * @brief  初始化电机硬件资源
  * @note   验证引脚配置有效性，设置默认方向（CW）、停止状态，
  *         并通过 HAL 抽象层初始化 GPIO、定时器、PWM 等硬件。
  *         调用成功后设置基类 initialized 标志。
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  * @retval MOTOR_ERR_CONFIG    引脚配置无效
  */
int Motor_Init(MotorBase *motor);

/**
  * @brief  运行电机主循环逻辑
  * @note   通过 vtable 调用子类实现的 run 函数。
  *         应在主循环中周期性调用，用于状态机刷新、位置更新等。
  *         若子类未实现 run，则返回 0（静默忽略）。
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  * @retval MOTOR_ERR_STATE     未初始化
  */
int Motor_Run(MotorBase *motor);

/**
  * @brief  复位电机至初始状态
  * @note   停止电机运转，将位置计数器归零，清除错误与限位标志。
  *         不清除引脚配置与类型信息。
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  */
int Motor_Reset(MotorBase *motor);

/**
  * @brief  立即停止电机
  * @note   释放 PWM 输出，将方向引脚置为安全电平，状态设为 MOTOR_STATE_STOPPED。
  *         不改变位置计数器（保留当前位置以供后续使用）。
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  */
int Motor_Stop(MotorBase *motor);

/**
  * @brief  设置电机旋转方向
  * @note   自动处理极性反转——若 dir_polarity_invert 为 1，
  *         实际写入硬件的方向将与参数相反。
  *         典型场景：硬件接线时将电机 A/B 相反接，设置反转标志即可
  *         在软件层面统一方向语义，无需改动接线。
  * @param  motor  指向电机基类对象的指针
  * @param  dir    目标方向
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  * @retval MOTOR_ERR_BUSY      电机运行中，不允许切换方向（需先停止）
  */
int Motor_SetDirection(MotorBase *motor, MotorDirection dir);

/**
  * @brief  获取电机当前状态快照
  * @note   填充 MotorStatus 结构体，包含运行状态、方向、位置、速度、限位等。
  * @param  motor   指向电机基类对象的指针
  * @param  status  指向状态结构体的指针（由调用者分配）
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  */
int Motor_GetStatus(MotorBase *motor, MotorStatus *status);

#endif /* __MOTOR_BASE_H__ */
