/**
  ******************************************************************************
  * @file    SensorBase.c
  * @brief   传感器基类实现 — 默认虚函数 + 构造/析构 + 公有接口
  *
  * @details
  * 提供 SensorBase 的默认虚函数实现和默认虚函数表实例。
  * 子类若未重写某个虚函数，将自动使用此文件中提供的默认行为。
  *
  * === 设计说明 ===
  *
  * 1. 默认虚函数表 (default_vtable)
  *    静态常量，所有指针指向本文件内的默认实现函数。
  *    子类构造后 vtable 指向此表，然后可替换为子类自定义表。
  *
  * 2. NULL 安全
  *    公有接口（SensorBase_Init/Run/Cleanup）均检查 self 和 vtable 指针，
  *    若为 NULL 则直接返回错误码，避免访存异常。
  *
  * 3. 初始化状态跟踪
  *    initialized 标志由 Init（置 1）和 Cleanup/Destructor（清 0）管理，
  *    避免重复初始化和重复清理。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "SensorBase.h"

/* ==================== 默认虚函数实现 ==================== */

/**
  * @brief  默认初始化函数（空实现）
  * @param  self  指向传感器对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int SensorBase_defaultInit(void *self)
{
    (void)self;
    return 0;
}

/**
  * @brief  默认运行函数（空实现）
  * @param  self  指向传感器对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int SensorBase_defaultRun(void *self)
{
    (void)self;
    return 0;
}

/**
  * @brief  默认清理函数（空实现）
  * @param  self  指向传感器对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int SensorBase_defaultCleanup(void *self)
{
    (void)self;
    return 0;
}

/**
  * @brief  默认复位函数（空实现）
  * @param  self  指向传感器对象自身的 void 指针
  */
static void SensorBase_defaultReset(void *self)
{
    (void)self;
}

/* ==================== 默认虚函数表实例 ==================== */

/**
  * @brief 默认虚函数表
  * @note  所有传感器对象构造后默认指向此表。
  *        子类定义自己的虚函数表并替换 vtable 指针即可实现多态。
  */
static const SensorVTable_t default_vtable = {
    .init    = SensorBase_defaultInit,
    .run     = SensorBase_defaultRun,
    .cleanup = SensorBase_defaultCleanup,
    .reset   = SensorBase_defaultReset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  传感器构造函数
  * @param  self  指向传感器基类对象的指针
  * @param  name  传感器名称字符串（仅保存指针，不拷贝）
  * @note   name 指向的字符串必须在传感器生命周期内有效。
  *         推荐使用字符串常量（如 "Encoder"、"Gyro"），而非局部变量。
  */
void SensorBase_Constructor(SensorBase_t *self, const char *name)
{
    if (self == NULL) {
        return;
    }

    self->vtable           = &default_vtable;
    self->name             = name;
    self->initialized      = 0;
    self->update_period_ms = 0;
}

/**
  * @brief  传感器析构函数
  * @param  self  指向传感器基类对象的指针
  * @note   若传感器已初始化，自动调用 cleanup 进行资源清理。
  *         最后清空 vtable 指针，防止悬空调用。
  */
void SensorBase_Destructor(SensorBase_t *self)
{
    if (self == NULL) {
        return;
    }

    if (self->initialized) {
        SensorBase_Cleanup(self);
    }

    self->vtable = NULL;
    self->name   = NULL;
}

/**
  * @brief  初始化传感器
  * @param  self  指向传感器基类对象的指针
  * @retval 0     成功
  * @retval -1    参数错误（self 或 vtable 为 NULL）
  * @retval 其他  子类 init 函数返回的错误码
  */
int SensorBase_Init(SensorBase_t *self)
{
    int ret = -1;

    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    if (self->initialized) {
        return 0;
    }

    if (self->vtable->init != NULL) {
        ret = self->vtable->init((void *)self);
    } else {
        ret = 0;
    }

    if (ret == 0) {
        self->initialized = 1;
    }

    return ret;
}

/**
  * @brief  运行传感器主逻辑（数据更新）
  * @param  self  指向传感器基类对象的指针
  * @retval 0     成功（或子类未实现 run）
  * @retval -1    参数错误
  * @retval 其他  子类 run 函数返回的错误码
  */
int SensorBase_Run(SensorBase_t *self)
{
    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    if (!self->initialized) {
        return -1;
    }

    if (self->vtable->run != NULL) {
        return self->vtable->run((void *)self);
    }

    return 0;
}

/**
  * @brief  清理传感器资源
  * @param  self  指向传感器基类对象的指针
  * @retval 0     成功
  * @retval -1    参数错误
  * @retval 其他  子类 cleanup 函数返回的错误码
  */
int SensorBase_Cleanup(SensorBase_t *self)
{
    int ret = 0;

    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    if (!self->initialized) {
        return 0;
    }

    if (self->vtable->cleanup != NULL) {
        ret = self->vtable->cleanup((void *)self);
    }

    self->initialized = 0;

    return ret;
}

/**
  * @brief  复位传感器数据
  * @param  self  指向传感器基类对象的指针
  * @note   通过 vtable 调用子类实现的 reset 函数。
  *         若子类未实现 reset，则静默忽略。
  */
void SensorBase_Reset(SensorBase_t *self)
{
    if ((self == NULL) || (self->vtable == NULL)) {
        return;
    }

    if (self->vtable->reset != NULL) {
        self->vtable->reset((void *)self);
    }
}

/**
  * @brief  获取传感器名称
  * @param  self  指向传感器基类对象的指针
  * @return const char*  传感器名称指针
  */
const char *SensorBase_GetName(const SensorBase_t *self)
{
    if (self == NULL) {
        return "NULL";
    }
    return (self->name != NULL) ? self->name : "UNKNOWN";
}

/**
  * @brief  查询传感器是否已初始化
  * @param  self  指向传感器基类对象的指针
  * @return uint8_t  0 = 未初始化, 1 = 已初始化
  */
uint8_t SensorBase_IsInitialized(const SensorBase_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->initialized;
}
