/**
 * @file device_base.h
 * @brief 通用设备驱动基类 - 实现虚函数指针表（多态）模式
 *
 * 这个模块定义所有设备驱动应遵循的通用接口，
 * 实现面向对象编程中的多态特性。
 *
 * 使用虚函数指针表（vtable）模式：
 * - 每个具体设备实现（如 Motor_t）包含一个操作表（如 MotorOps_t）
 * - 操作表中定义了设备的所有操作函数指针
 * - 通过函数指针调用实现接口多态
 *
 * 示例结构：
 *   Motor_t {
 *       MotorOps_t *ops;    // 虚函数表
 *       uint16_t pwm;       // 私有数据
 *   };
 *
 *   MotorOps_t {
 *       void (*init)(Motor_t *);
 *       void (*set_speed)(Motor_t *, uint16_t);
 *   };
 */

#ifndef DEVICE_BASE_H
#define DEVICE_BASE_H

#include <stdint.h>

/**
 * @brief 通用设备操作接口
 * 所有具体的操作表应包含以下通用操作
 */
typedef struct {
    void (*init)(void *dev);                  /**< 初始化设备 */
    int (*deinit)(void *dev);                 /**< 反初始化设备 */
    int (*read)(void *dev, void *buffer, uint32_t size);  /**< 读取数据 */
    int (*write)(void *dev, const void *buffer, uint32_t size);  /**< 写入数据 */
} DeviceOps_t;

/**
 * @brief 基础设备结构体
 * 具体的设备应包含此结构体作为成员
 */
typedef struct {
    const char *name;           /**< 设备名称 */
    uint8_t id;                 /**< 设备 ID */
    uint8_t initialized;        /**< 初始化状态标志 */
    const DeviceOps_t *ops;     /**< 操作接口指针 */
} Device_t;

/**
 * @brief 获取设备操作表
 */
#define device_get_ops(dev) ((dev)->ops)

/**
 * @brief 调用设备操作的宏定义，简化语法
 */
#define device_init(dev) \
    do { if ((dev)->ops && (dev)->ops->init) (dev)->ops->init(dev); } while(0)

#define device_deinit(dev) \
    do { if ((dev)->ops && (dev)->ops->deinit) (dev)->ops->deinit(dev); } while(0)

#define device_read(dev, buf, size) \
    ((dev)->ops && (dev)->ops->read) ? (dev)->ops->read(dev, buf, size) : -1

#define device_write(dev, buf, size) \
    ((dev)->ops && (dev)->ops->write) ? (dev)->ops->write(dev, buf, size) : -1

#endif // DEVICE_BASE_H
