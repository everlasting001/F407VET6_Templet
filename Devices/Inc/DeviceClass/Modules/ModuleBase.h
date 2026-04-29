/**
  ******************************************************************************
  * @file    ModuleBase.h
  * @brief   设备模块基类 — 使用 C 语言模拟面向对象（虚函数表 + 构造/析构模式）
  *
  * @details
  * 本文件定义了所有设备模块（如 LED、KEY、OLED、BUZZER 等）的基类接口。
  * 通过结构体 + 函数指针（虚函数表）的方式实现 C 语言多态。
  *
  * === 使用示例 ===
  *
  * // 1. 定义子类结构体（将 ModuleBase_t 作为第一个成员，模拟继承）
  * typedef struct {
  *     ModuleBase_t base;      // 基类（必须为第一个成员）
  *     // ... 子类自有成员
  * } LED_t;
  *
  * // 2. 实现子类的虚函数
  * static int LED_init(void *self) {
  *     LED_t *led = (LED_t *)self;
  *     // ... 初始化逻辑
  *     return 0;
  * }
  *
  * // 3. 定义子类的虚函数表
  * static const ModuleVTable_t led_vtable = {
  *     .init    = LED_init,
  *     .run     = NULL,      // 未实现则使用默认
  *     .cleanup = NULL,
  * };
  *
  * // 4. 构造子类对象
  * LED_t led;
  * ModuleBase_Constructor(&led.base, "LED");
  * led.base.vtable = &led_vtable;  // 替换为子类虚函数表
  *
  * // 5. 通过基类指针多态调用
  * ModuleBase_t *p = (ModuleBase_t *)&led;
  * ModuleBase_Init(p);   // 实际调用 LED_init
  * ModuleBase_Run(p);
  * ModuleBase_Cleanup(p);
  * ModuleBase_Destructor(p);
  *
  ******************************************************************************
  */

#ifndef __MODULE_BASE_H__
#define __MODULE_BASE_H__

#include <stdint.h>

/* ==================== 虚函数表（VTable）定义 ==================== */

/**
  * @brief 模块虚函数表
  *
  * 子类通过提供自定义实现来重写这些虚函数。
  * 若某个函数指针为 NULL，则调用时会自动使用基类的默认实现。
  *
  * @param self  指向模块对象自身的 void 指针（子类结构体指针）
  * @return int  0 = 成功, 负数 = 错误码
  */
typedef struct {
    int (*init)(void *self);        /**< 初始化模块（在构造函数后调用）*/
    int (*run)(void *self);         /**< 模块主循环/运行逻辑 */
    int (*cleanup)(void *self);     /**< 清理模块资源 */
    void (*reset)(void *self);      /**< 复位模块到初始状态 */
} ModuleVTable_t;

/* ==================== 基类结构体定义 ==================== */

/**
  * @brief 模块基类结构体
  *
  * 子类必须将此结构体作为第一个成员，确保指针可以安全转换。
  * 这模拟了 C++ 的单继承：派生类对象指针 = 基类对象指针。
  */
typedef struct ModuleBase_s {
    const ModuleVTable_t *vtable;   /**< 虚函数表指针（指向子类或默认 vtable）*/
    const char           *name;     /**< 模块名称（调试标识）*/
    uint8_t               initialized; /**< 初始化标志 (0=未初始化, 1=已初始化) */
} ModuleBase_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  模块构造函数
  * @note   必须在任何模块操作之前调用。
  *         初始化 vtable 为默认表，设置模块名称，清空 initialized 标志。
  * @param  self  指向模块基类对象的指针
  * @param  name  模块名称字符串（仅保存指针，不拷贝）
  */
void ModuleBase_Constructor(ModuleBase_t *self, const char *name);

/**
  * @brief  模块析构函数
  * @note   在模块生命周期结束时调用。
  *         若已初始化则自动调用 cleanup，最后清空 vtable 指针。
  * @param  self  指向模块基类对象的指针
  */
void ModuleBase_Destructor(ModuleBase_t *self);

/**
  * @brief  初始化模块
  * @note   通过 vtable 调用子类实现的 init 函数。
  *         调用成功后设置 initialized = 1。
  * @param  self  指向模块基类对象的指针
  * @return int   0 = 成功, -1 = 参数错误, 其他负数 = 子类错误码
  */
int ModuleBase_Init(ModuleBase_t *self);

/**
  * @brief  运行模块主逻辑
  * @note   通过 vtable 调用子类实现的 run 函数。
  *         若子类未实现 run，则返回 0（静默忽略）。
  * @param  self  指向模块基类对象的指针
  * @return int   0 = 成功, 负数 = 错误码
  */
int ModuleBase_Run(ModuleBase_t *self);

/**
  * @brief  清理模块资源
  * @note   通过 vtable 调用子类实现的 cleanup 函数。
  *         调用后清空 initialized 标志。
  * @param  self  指向模块基类对象的指针
  * @return int   0 = 成功, 负数 = 错误码
  */
int ModuleBase_Cleanup(ModuleBase_t *self);

/**
  * @brief  获取模块名称
  * @param  self  指向模块基类对象的指针
  * @return const char* 模块名称指针（self 为 NULL 时返回 "NULL"）
  */
const char *ModuleBase_GetName(const ModuleBase_t *self);

/**
  * @brief  查询模块是否已初始化
  * @param  self  指向模块基类对象的指针
  * @return uint8_t  0 = 未初始化, 1 = 已初始化
  */
uint8_t ModuleBase_IsInitialized(const ModuleBase_t *self);

#endif /* __MODULE_BASE_H__ */
