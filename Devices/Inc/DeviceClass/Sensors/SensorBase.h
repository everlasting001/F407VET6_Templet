/**
  ******************************************************************************
  * @file    SensorBase.h
  * @brief   传感器基类 — C 语言模拟面向对象（虚函数表 + 构造/析构模式）
  *
  * @details
  * 本文件定义了所有传感器模块（如 Encoder、Gyro、LineSensor 等）的基类接口。
  * 通过结构体 + 函数指针（虚函数表）的方式实现 C 语言多态。
  *
  * 基类提供 name、initialized、update_period_ms 等传感器共用属性，
  * 子类通过 SensorBase_Constructor() 初始化基类成员并替换 vtable。
  *
  * === 使用示例 ===
  *
  * // 1. 定义子类结构体（将 SensorBase_t 作为第一个成员，模拟继承）
  * typedef struct {
  *     SensorBase_t base;      // 基类（必须为第一个成员）
  *     // ... 子类自有成员
  * } Encoder_t;
  *
  * // 2. 实现子类的虚函数
  * static int Encoder_init(void *self) {
  *     Encoder_t *enc = (Encoder_t *)self;
  *     // ... 初始化逻辑
  *     return 0;
  * }
  *
  * // 3. 定义子类的虚函数表
  * static const SensorVTable_t encoder_vtable = {
  *     .init    = Encoder_init,
  *     .run     = Encoder_run,
  *     .cleanup = Encoder_cleanup,
  *     .reset   = Encoder_reset,
  * };
  *
  * // 4. 构造子类对象
  * Encoder_t enc;
  * SensorBase_Constructor(&enc.base, "Encoder");
  * enc.base.vtable = &encoder_vtable;
  *
  * // 5. 通过基类指针多态调用
  * SensorBase_t *p = (SensorBase_t *)&enc;
  * SensorBase_Init(p);   // 实际调用 Encoder_init
  * SensorBase_Run(p);
  * SensorBase_Cleanup(p);
  *
  ******************************************************************************
  */

#ifndef __SENSOR_BASE_H__
#define __SENSOR_BASE_H__

#include <stdint.h>
#include <stddef.h>

/* ==================== 虚函数表（VTable）定义 ==================== */

/**
  * @brief 传感器虚函数表
  *
  * 子类通过提供自定义实现来重写这些虚函数。
  * 若某个函数指针为 NULL，则调用时会自动使用基类的默认实现。
  *
  * @param self  指向传感器对象自身的 void 指针（子类结构体指针）
  * @return int  0 = 成功, 负数 = 错误码
  */
typedef struct {
    int (*init)(void *self);        /**< 初始化传感器（在构造函数后调用）*/
    int (*run)(void *self);         /**< 传感器数据更新/运行逻辑 */
    int (*cleanup)(void *self);     /**< 清理传感器资源 */
    void (*reset)(void *self);      /**< 复位传感器到初始状态 */
} SensorVTable_t;

/* ==================== 基类结构体定义 ==================== */

/**
  * @brief 传感器基类结构体
  *
  * 子类必须将此结构体作为第一个成员，确保指针可以安全转换。
  * 模拟了 C++ 的单继承：派生类对象指针 = 基类对象指针。
  *
  * update_period_ms 是所有定时采样的传感器共用的属性，
  * 提取到基类中避免了每个子类重复定义。
  */
typedef struct SensorBase_s {
    const SensorVTable_t *vtable;          /**< 虚函数表指针（指向子类或默认 vtable）*/
    const char            *name;           /**< 传感器名称（调试标识）*/
    uint8_t                initialized;    /**< 初始化标志 (0=未初始化, 1=已初始化) */
    uint32_t               update_period_ms; /**< 传感器更新周期 (ms)，0 表示非周期采样 */
} SensorBase_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  传感器构造函数
  * @note   必须在任何传感器操作之前调用。
  *         初始化 vtable 为默认表，设置传感器名称，清空 initialized 标志。
  *         update_period_ms 初始化为 0，子类可在构造函数中设置。
  * @param  self  指向传感器基类对象的指针
  * @param  name  传感器名称字符串（仅保存指针，不拷贝）
  */
void SensorBase_Constructor(SensorBase_t *self, const char *name);

/**
  * @brief  传感器析构函数
  * @note   在传感器生命周期结束时调用。
  *         若已初始化则自动调用 cleanup，最后清空 vtable 指针。
  * @param  self  指向传感器基类对象的指针
  */
void SensorBase_Destructor(SensorBase_t *self);

/**
  * @brief  初始化传感器
  * @note   通过 vtable 调用子类实现的 init 函数。
  *         调用成功后设置 initialized = 1。
  * @param  self  指向传感器基类对象的指针
  * @return int   0 = 成功, -1 = 参数错误, 其他负数 = 子类错误码
  */
int SensorBase_Init(SensorBase_t *self);

/**
  * @brief  运行传感器主逻辑（数据更新）
  * @note   通过 vtable 调用子类实现的 run 函数。
  *         若子类未实现 run，则返回 0（静默忽略）。
  * @param  self  指向传感器基类对象的指针
  * @return int   0 = 成功, 负数 = 错误码
  */
int SensorBase_Run(SensorBase_t *self);

/**
  * @brief  清理传感器资源
  * @note   通过 vtable 调用子类实现的 cleanup 函数。
  *         调用后清空 initialized 标志。
  * @param  self  指向传感器基类对象的指针
  * @return int   0 = 成功, 负数 = 错误码
  */
int SensorBase_Cleanup(SensorBase_t *self);

/**
  * @brief  复位传感器数据（子类实现）
  * @note   通过 vtable 调用子类实现的 reset 函数。
  *         典型行为：清零累积数据，不改变硬件配置。
  * @param  self  指向传感器基类对象的指针
  */
void SensorBase_Reset(SensorBase_t *self);

/**
  * @brief  获取传感器名称
  * @param  self  指向传感器基类对象的指针
  * @return const char* 传感器名称指针（self 为 NULL 时返回 "NULL"）
  */
const char *SensorBase_GetName(const SensorBase_t *self);

/**
  * @brief  查询传感器是否已初始化
  * @param  self  指向传感器基类对象的指针
  * @return uint8_t  0 = 未初始化, 1 = 已初始化
  */
uint8_t SensorBase_IsInitialized(const SensorBase_t *self);

#endif /* __SENSOR_BASE_H__ */
