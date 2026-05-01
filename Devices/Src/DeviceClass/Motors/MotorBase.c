/**
  ******************************************************************************
  * @file    MotorBase.c
  * @brief   电机模块基类实现 — 默认虚函数 + 公有接口 + 默认 HAL 实现
  *
  * @details
  * 提供 MotorBase 的默认虚函数实现、默认 HAL 回调实现以及公有接口函数。
  * 子类（DCMotor、StepMotor、Servo）通过替换虚函数表和 HAL 回调实现多态。
  *
  * === 设计说明 ===
  *
  * 1. 继承链：MotorBase → ModuleBase
  *    MotorBase 自身拥有一套默认虚函数表，子类可在此基础上进一步重写。
  *
  * 2. 默认 HAL 实现
  *    本文件提供基于 STM32 HAL 库的默认硬件抽象层实现。
  *    子类或应用层可通过替换 motor->hal 指针来适配不同平台。
  *
  * 3. 方向极性反转
  *    Motor_SetDirection() 内部自动处理极性反转逻辑，
  *    子类无需关心硬件接线差异。
  *
  * 4. 参数校验
  *    所有公有接口均进行空指针与有效性检查，通过错误码返回异常。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "MotorBase.h"
#include "stm32f4xx_hal.h"      /* HAL_GPIO_WritePin, HAL_GetTick, HAL_Delay */

/* ==================== 私有宏定义 ==================== */

/** @defgroup Motor_Private_Macros 私有宏
  * @{
  */
#define MOTOR_LIMIT_POS_BIT    0x01U   /**< 正向限位标志位 */
#define MOTOR_LIMIT_NEG_BIT    0x02U   /**< 反向限位标志位 */
/**
  * @}
  */

/* ==================== 默认 HAL 回调实现 ==================== */

/**
  * @brief  默认 GPIO 写操作（基于 STM32 HAL 库）
  * @param  port   GPIO 端口
  * @param  pin    GPIO 引脚号
  * @param  state  0 = 低电平, 非 0 = 高电平
  * @retval 0      成功
  */
static int MotorHAL_defaultGpioWrite(GPIO_TypeDef *port, uint16_t pin, uint8_t state)
{
    if ((port == NULL) || (pin == 0)) {
        return MOTOR_ERR_CONFIG;
    }

    GPIO_PinState hal_state = (state != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(port, pin, hal_state);
    return MOTOR_OK;
}

/**
  * @brief  默认 PWM 启动（空实现，由子类重写）
  * @note   基类不绑定具体定时器，子类需提供自己的定时器配置。
  * @param  timer    定时器句柄指针
  * @param  channel  定时器通道
  * @retval 0        成功
  */
static int MotorHAL_defaultPwmStart(void *timer, uint32_t channel)
{
    (void)timer;
    (void)channel;
    /* 基类无默认 PWM 配置，子类应替换此回调 */
    return MOTOR_OK;
}

/**
  * @brief  默认 PWM 停止（空实现，由子类重写）
  * @param  timer    定时器句柄指针
  * @param  channel  定时器通道
  * @retval 0        成功
  */
static int MotorHAL_defaultPwmStop(void *timer, uint32_t channel)
{
    (void)timer;
    (void)channel;
    return MOTOR_OK;
}

/**
  * @brief  默认 PWM 占空比设置（空实现，由子类重写）
  * @param  timer    定时器句柄指针
  * @param  channel  定时器通道
  * @param  duty     占空比值
  * @retval 0        成功
  */
static int MotorHAL_defaultPwmSetDuty(void *timer, uint32_t channel, uint32_t duty)
{
    (void)timer;
    (void)channel;
    (void)duty;
    return MOTOR_OK;
}

/**
  * @brief  默认微秒延时（基于 HAL_Delay 实现，精度为 1ms）
  * @note   若需要精确微秒延时，子类应替换为 DWT 或定时器实现。
  *         此处以 ms 为最小单位，ceil(us/1000) 保证至少等待指定时长。
  * @param  us  延时时长（微秒）
  */
static void MotorHAL_defaultDelayUs(uint32_t us)
{
    uint32_t ms = (us + 999U) / 1000U;
    if (ms > 0) {
        HAL_Delay(ms);
    }
}

/**
  * @brief  默认系统滴答获取
  * @return uint32_t  当前系统滴答（毫秒）
  */
static uint32_t MotorHAL_defaultGetTick(void)
{
    return HAL_GetTick();
}

/**
  * @brief 默认硬件抽象层实例（基于 STM32 HAL）
  */
static const MotorHAL default_hal = {
    .gpio_write   = MotorHAL_defaultGpioWrite,
    .pwm_start    = MotorHAL_defaultPwmStart,
    .pwm_stop     = MotorHAL_defaultPwmStop,
    .pwm_set_duty = MotorHAL_defaultPwmSetDuty,
    .delay_us     = MotorHAL_defaultDelayUs,
    .get_tick     = MotorHAL_defaultGetTick,
};

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  计算结构体简单校验和
  * @note   用于结构体版本/完整性校验，防止内存损坏导致的异常行为。
  * @param  motor  指向电机对象的指针
  * @return uint16_t  校验和（version 和 checksum 字段本身不参与计算）
  */
static uint16_t Motor_ComputeChecksum(const MotorBase *motor)
{
    uint16_t sum = 0;
    const uint8_t *p = (const uint8_t *)motor;
    size_t len = sizeof(MotorBase);

    /* 跳过 checksum 字段所在的 2 字节 */
    size_t checksum_offset = offsetof(MotorBase, checksum);
    for (size_t i = 0; i < len; i++) {
        if (i >= checksum_offset && i < checksum_offset + sizeof(uint16_t)) {
            continue;
        }
        sum += p[i];
    }
    return sum;
}

/**
  * @brief  验证电机对象参数有效性
  * @param  motor  指向电机对象的指针
  * @retval 0      有效
  * @retval 负数   错误码
  */
static int Motor_Validate(const MotorBase *motor)
{
    if (motor == NULL) {
        return MOTOR_ERR_NULL;
    }

    if (motor->version != MOTOR_BASE_VERSION) {
        return MOTOR_ERR_CONFIG;
    }

    if (motor->pin_count == 0 || motor->pin_count > MOTOR_PIN_MAX) {
        return MOTOR_ERR_CONFIG;
    }

    return MOTOR_OK;
}

/* ==================== 虚函数实现（默认行为） ==================== */

/**
  * @brief  电机初始化虚函数（默认实现）
  * @note   验证引脚有效性，将所有引脚初始化为低电平安全状态。
  *         子类可重写此函数以添加定时器、PWM 等初始化。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    参数无效或配置错误
  */
static int Motor_init(void *self)
{
    MotorBase *motor = (MotorBase *)self;

    int ret = Motor_Validate(motor);
    if (ret != MOTOR_OK) {
        return -1;
    }

    if (motor->hal == NULL) {
        return -1;
    }

    /* 将所有引脚初始化为低电平（安全状态） */
    for (uint8_t i = 0; i < motor->pin_count; i++) {
        if (motor->pins[i].Port != NULL && motor->pins[i].Pin != 0) {
            motor->hal->gpio_write(motor->pins[i].Port, motor->pins[i].Pin, 0);
        }
    }

    /* 设置默认状态 */
    motor->state           = MOTOR_STATE_STOPPED;
    motor->direction       = MOTOR_DIR_CW;
    motor->speed           = 0;
    motor->position        = 0;
    motor->target_position = 0;
    motor->limit_flags     = 0;

    /* 计算校验和 */
    motor->checksum = Motor_ComputeChecksum(motor);

    return 0;
}

/**
  * @brief  电机运行虚函数（默认实现）
  * @note   基类默认无周期运行逻辑，子类应重写此函数实现状态机刷新。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    参数无效
  */
static int Motor_run(void *self)
{
    MotorBase *motor = (MotorBase *)self;
    (void)motor;
    return 0;
}

/**
  * @brief  电机清理虚函数（默认实现）
  * @note   停止电机，将所有引脚设为低电平，清除状态。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int Motor_cleanup(void *self)
{
    MotorBase *motor = (MotorBase *)self;

    if (motor->hal != NULL) {
        /* 停止 PWM */
        motor->hal->pwm_stop(motor->timer_handle, motor->pwm_channel);

        /* 所有引脚输出低电平（安全状态） */
        for (uint8_t i = 0; i < motor->pin_count; i++) {
            if (motor->pins[i].Port != NULL && motor->pins[i].Pin != 0) {
                motor->hal->gpio_write(motor->pins[i].Port, motor->pins[i].Pin, 0);
            }
        }
    }

    motor->state = MOTOR_STATE_STOPPED;
    motor->speed = 0;

    return 0;
}

/**
  * @brief  电机复位虚函数（默认实现）
  * @note   复位到初始状态：停止、位置归零、清空限位标志。
  * @param  self  指向模块对象自身的 void 指针
  */
static void Motor_reset(void *self)
{
    MotorBase *motor = (MotorBase *)self;

    /* 先停止 */
    Motor_cleanup(self);

    /* 复位位置和标志 */
    motor->position        = 0;
    motor->target_position = 0;
    motor->limit_flags     = 0;
    motor->direction       = MOTOR_DIR_CW;
}

/* ==================== 子类虚函数表实例 ==================== */

/**
  * @brief 电机模块默认虚函数表
  * @note  所有虚函数均已重写。子类可继承并替换部分函数实现差异化行为。
  */
static const MotorVTable motor_vtable = {
    .init    = Motor_init,
    .run     = Motor_run,
    .cleanup = Motor_cleanup,
    .reset   = Motor_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  电机构造函数
  * @param  self      指向电机基类对象的指针
  * @param  name      电机名称
  * @param  type      电机类型
  * @param  pins      引脚配置数组指针
  * @param  pin_count 引脚数量
  */
void Motor_Constructor(MotorBase *self, const char *name, MotorType type,
                       const GPIO_PinConfig *pins, uint8_t pin_count)
{
    if (self == NULL) {
        return;
    }

    /* 1. 初始化 vtable 和基础成员 */
    self->vtable      = &motor_vtable;
    self->name        = name;
    self->initialized = 0;
    self->port        = NULL;
    self->pin         = 0;

    /* 2. 设置电机类型 */
    self->type = type;

    /* 3. 拷贝引脚配置（限幅处理） */
    if (pin_count > MOTOR_PIN_MAX) {
        pin_count = MOTOR_PIN_MAX;
    }
    self->pin_count = pin_count;

    if (pins != NULL && pin_count > 0) {
        for (uint8_t i = 0; i < pin_count; i++) {
            self->pins[i] = pins[i];
        }
    }

    /* 4. 初始化电机专用成员为默认值 */
    self->direction           = MOTOR_DIR_CW;
    self->dir_polarity_invert = MOTOR_POLARITY_NORMAL;
    self->state               = MOTOR_STATE_STOPPED;
    self->speed               = 0;
    self->position            = 0;
    self->target_position     = 0;
    self->limit_flags         = 0;
    self->version             = MOTOR_BASE_VERSION;
    self->hal                 = &default_hal;
    self->timer_handle        = NULL;
    self->pwm_channel         = 0;

    /* 5. 计算初始校验和 */
    self->checksum = 0;
    self->checksum = Motor_ComputeChecksum(self);

    /* 6. 虚函数表已在步骤1中设置 */
}

/**
  * @brief  初始化电机硬件资源
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  * @retval MOTOR_ERR_CONFIG    引脚配置无效
  */
int Motor_Init(MotorBase *motor)
{
    if (motor == NULL) {
        return MOTOR_ERR_NULL;
    }

    if (motor->version != MOTOR_BASE_VERSION) {
        return MOTOR_ERR_CONFIG;
    }

    if (motor->hal == NULL) {
        return MOTOR_ERR_CONFIG;
    }

    if (motor->pin_count == 0 || motor->pin_count > MOTOR_PIN_MAX) {
        return MOTOR_ERR_CONFIG;
    }

    /* 防止重复初始化 */
    if (motor->initialized) {
        return MOTOR_OK;
    }

    /* 通过虚函数表调用子类 init（若为 NULL 则跳过） */
    int ret = 0;
    if (motor->vtable != NULL && motor->vtable->init != NULL) {
        ret = motor->vtable->init((void *)motor);
    }

    /* 初始化成功后设置标志 */
    if (ret == 0) {
        motor->initialized = 1;
        return MOTOR_OK;
    }

    return MOTOR_ERR_CONFIG;
}

/**
  * @brief  运行电机主循环逻辑
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  * @retval MOTOR_ERR_STATE     未初始化
  */
int Motor_Run(MotorBase *motor)
{
    if (motor == NULL) {
        return MOTOR_ERR_NULL;
    }

    if (motor->vtable == NULL) {
        return MOTOR_ERR_NULL;
    }

    if (!motor->initialized) {
        return MOTOR_ERR_STATE;
    }

    if (motor->vtable->run != NULL) {
        return motor->vtable->run((void *)motor);
    }

    return MOTOR_OK;
}

/**
  * @brief  复位电机至初始状态
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK       成功
  * @retval MOTOR_ERR_NULL 参数为空
  */
int Motor_Reset(MotorBase *motor)
{
    if (motor == NULL) {
        return MOTOR_ERR_NULL;
    }

    /* 停止电机并复位状态 */
    Motor_reset((void *)motor);

    return MOTOR_OK;
}

/**
  * @brief  立即停止电机
  * @param  motor  指向电机基类对象的指针
  * @retval MOTOR_OK       成功
  * @retval MOTOR_ERR_NULL 参数为空
  */
int Motor_Stop(MotorBase *motor)
{
    if (motor == NULL) {
        return MOTOR_ERR_NULL;
    }

    if (motor->hal == NULL) {
        motor->state = MOTOR_STATE_STOPPED;
        motor->speed = 0;
        return MOTOR_OK;
    }

    /* 停止 PWM 输出 */
    motor->hal->pwm_stop(motor->timer_handle, motor->pwm_channel);

    /* 将引脚置为安全电平 */
    for (uint8_t i = 0; i < motor->pin_count; i++) {
        if (motor->pins[i].Port != NULL && motor->pins[i].Pin != 0) {
            motor->hal->gpio_write(motor->pins[i].Port, motor->pins[i].Pin, 0);
        }
    }

    motor->state = MOTOR_STATE_STOPPED;
    motor->speed = 0;

    return MOTOR_OK;
}

/**
  * @brief  设置电机旋转方向
  * @note   自动处理极性反转。若电机正在运行，返回 MOTOR_ERR_BUSY。
  *         子类可放宽此限制（先停止、切换方向、再启动）。
  * @param  motor  指向电机基类对象的指针
  * @param  dir    目标方向
  * @retval MOTOR_OK            成功
  * @retval MOTOR_ERR_NULL      参数为空
  * @retval MOTOR_ERR_BUSY      电机运行中，不允许切换方向
  */
int Motor_SetDirection(MotorBase *motor, MotorDirection dir)
{
    if (motor == NULL) {
        return MOTOR_ERR_NULL;
    }

    if (motor->state == MOTOR_STATE_RUNNING) {
        return MOTOR_ERR_BUSY;
    }

    /* 存储逻辑方向，由子类在输出时调用 Motor_ResolveDirection 获取硬件方向 */
    motor->direction = dir;

    return MOTOR_OK;
}

/**
  * @brief  获取电机当前状态快照
  * @param  motor   指向电机基类对象的指针
  * @param  status  指向状态结构体的指针（由调用者分配）
  * @retval MOTOR_OK       成功
  * @retval MOTOR_ERR_NULL 参数为空
  */
int Motor_GetStatus(MotorBase *motor, MotorStatus *status)
{
    if ((motor == NULL) || (status == NULL)) {
        return MOTOR_ERR_NULL;
    }

    status->state          = motor->state;
    status->direction      = motor->direction;
    status->position       = motor->position;
    status->speed          = motor->speed;
    status->limit_flags    = motor->limit_flags;
    status->is_initialized = motor->initialized;

    return MOTOR_OK;
}
