/**
  ******************************************************************************
  * @file    ModuleBase.c
  * @brief   设备模块基类实现 — 默认虚函数 + 构造/析构 + 公有接口
  *
  * @details
  * 提供 ModuleBase 的默认虚函数实现和默认虚函数表实例。
  * 子类若未重写某个虚函数，将自动使用此文件中提供的默认行为。
  *
  * === 设计说明 ===
  *
  * 1. 默认虚函数表 (default_vtable)
  *    静态常量，所有指针指向本文件内的默认实现函数。
  *    子类构造后 vtable 指向此表，然后可替换为子类自定义表。
  *
  * 2. NULL 安全
  *    公有接口（ModuleBase_Init/Run/Cleanup）均检查 self 和 vtable 指针，
  *    若为 NULL 则直接返回错误码，避免访存异常。
  *
  * 3. 初始化状态跟踪
  *    initialized 标志由 Init（置 1）和 Cleanup/Destructor（清 0）管理，
  *    避免重复初始化和重复清理。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ModuleBase.h"

/* ==================== 默认虚函数实现 ==================== */

/**
  * @brief  默认初始化函数（空实现）
  * @note   子类若不需要初始化逻辑，可不重写此函数。
  *         默认行为：仅返回成功，不执行任何操作。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int ModuleBase_defaultInit(void *self)
{
    (void)self;  /* 未使用参数，避免编译器警告 */
    return 0;
}

/**
  * @brief  默认运行函数（空实现）
  * @note   子类必须重写此函数以实现具体运行逻辑。
  *         默认行为：仅返回成功，不执行任何操作。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int ModuleBase_defaultRun(void *self)
{
    (void)self;
    return 0;
}

/**
  * @brief  默认清理函数（空实现）
  * @note   子类若不需要清理逻辑，可不重写此函数。
  *         默认行为：仅返回成功，不执行任何操作。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int ModuleBase_defaultCleanup(void *self)
{
    (void)self;
    return 0;
}

/**
  * @brief  默认复位函数（空实现）
  * @note   子类若需要复位功能，应重写此函数。
  *         默认行为：仅返回成功，不执行任何操作。
  * @param  self  指向模块对象自身的 void 指针
  */
static void ModuleBase_defaultReset(void *self)
{
    (void)self;
}

/* ==================== 默认虚函数表实例 ==================== */

/**
  * @brief 默认虚函数表
  * @note  所有模块对象构造后默认指向此表。
  *        子类定义自己的虚函数表并替换 vtable 指针即可实现多态。
  */
static const ModuleVTable_t default_vtable = {
    .init    = ModuleBase_defaultInit,
    .run     = ModuleBase_defaultRun,
    .cleanup = ModuleBase_defaultCleanup,
    .reset   = ModuleBase_defaultReset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  模块构造函数
  * @param  self  指向模块基类对象的指针
  * @param  name  模块名称字符串（仅保存指针，不拷贝）
  * @note   name 指向的字符串必须在模块生命周期内有效。
  *         推荐使用字符串常量（如 "LED"、"KEY"），而非局部变量。
  */
void ModuleBase_Constructor(ModuleBase_t *self, const char *name)
{
    if (self == NULL) {
        return;  /* 防止空指针写入 */
    }

    /* 指向默认虚函数表，子类可在构造后替换 */
    self->vtable      = &default_vtable;
    self->name        = name;
    self->initialized = 0;
}

/**
  * @brief  模块析构函数
  * @param  self  指向模块基类对象的指针
  * @note   若模块已初始化，自动调用 cleanup 进行资源清理。
  *         最后清空 vtable 指针，防止悬空调用。
  */
void ModuleBase_Destructor(ModuleBase_t *self)
{
    if (self == NULL) {
        return;
    }

    /* 若已初始化，自动清理 */
    if (self->initialized) {
        ModuleBase_Cleanup(self);
    }

    /* 清空虚函数表指针，防止悬空指针 */
    self->vtable = NULL;
    self->name   = NULL;
}

/**
  * @brief  初始化模块
  * @param  self  指向模块基类对象的指针
  * @retval 0     成功
  * @retval -1    参数错误（self 或 vtable 为 NULL）
  * @retval 其他  子类 init 函数返回的错误码
  */
int ModuleBase_Init(ModuleBase_t *self)
{
    int ret = -1;

    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;  /* 参数无效 */
    }

    /* 防止重复初始化 */
    if (self->initialized) {
        return 0;
    }

    /* 通过虚函数表调用子类 init（若为 NULL 则跳过） */
    if (self->vtable->init != NULL) {
        ret = self->vtable->init((void *)self);
    } else {
        ret = 0;
    }

    /* 初始化成功后设置标志 */
    if (ret == 0) {
        self->initialized = 1;
    }

    return ret;
}

/**
  * @brief  运行模块主逻辑
  * @param  self  指向模块基类对象的指针
  * @retval 0     成功（或子类未实现 run）
  * @retval -1    参数错误
  * @retval 其他  子类 run 函数返回的错误码
  */
int ModuleBase_Run(ModuleBase_t *self)
{
    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    /* 未初始化时不允许运行 */
    if (!self->initialized) {
        return -1;
    }

    /* 通过虚函数表调用子类 run（允许为 NULL） */
    if (self->vtable->run != NULL) {
        return self->vtable->run((void *)self);
    }

    return 0;  /* 子类未实现 run，静默忽略 */
}

/**
  * @brief  清理模块资源
  * @param  self  指向模块基类对象的指针
  * @retval 0     成功
  * @retval -1    参数错误
  * @retval 其他  子类 cleanup 函数返回的错误码
  */
int ModuleBase_Cleanup(ModuleBase_t *self)
{
    int ret = 0;

    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    /* 未初始化时无需清理 */
    if (!self->initialized) {
        return 0;
    }

    /* 通过虚函数表调用子类 cleanup（若为 NULL 则跳过） */
    if (self->vtable->cleanup != NULL) {
        ret = self->vtable->cleanup((void *)self);
    }

    /* 无论清理成功与否，都清除初始化标志 */
    self->initialized = 0;

    return ret;
}

/**
  * @brief  获取模块名称
  * @param  self  指向模块基类对象的指针
  * @return const char*  模块名称指针
  */
const char *ModuleBase_GetName(const ModuleBase_t *self)
{
    if (self == NULL) {
        return "NULL";
    }
    return (self->name != NULL) ? self->name : "UNKNOWN";
}

/**
  * @brief  查询模块是否已初始化
  * @param  self  指向模块基类对象的指针
  * @return uint8_t  0 = 未初始化, 1 = 已初始化
  */
uint8_t ModuleBase_IsInitialized(const ModuleBase_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->initialized;
}
