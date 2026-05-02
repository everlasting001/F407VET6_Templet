/**
  ******************************************************************************
  * @file    Gyro.h
  * @brief   陀螺仪传感器子类 — 继承 SensorBase 的 MPU6050 六轴 IMU 实现
  *
  * @details
  * 本文件定义了陀螺仪模块的结构体和公有接口，继承自 SensorBase 基类。
  * 基类提供 name、initialized、update_period_ms 等属性，子类专注实现:
  *   - MPU6050 I2C 寄存器读写 (HAL I2C, 100kHz)
  *   - Z 轴角速度读取与零漂门限滤波 (±0.4°/s)
  *   - 航向角 (Yaw) 半积分累积
  *   - 角度归一化与复位
  *   - 调试打印 (通过 DebugPrintf DMA 发送)
  *
  * === 硬件配置 ===
  *
  * I2C 总线:     I2C1 (PB6=SCL, PB7=SDA)
  * 设备地址:     0x68 (7-bit)
  * 时钟频率:     100kHz
  * 陀螺仪量程:   ±500 °/s (131 LSB/°/s)
  * 低通滤波:     42Hz (DLPF_CFG=3)
  * 采样率:       1kHz / (1+9) = 100Hz (SMPLRT_DIV=9)
  *
  * === ISR 安全 ===
  *
  * SensorBase_Run() 内部调用 HAL I2C 阻塞读写 (~200μs)，
  * 因此不可在 TIM ISR 中调用。必须在主循环 while(1) 中以
  * 软件门控周期调用（推荐 5Hz / 200ms）。
  *
  * === 使用示例 ===
  *
  * // 1. 构造
  * Gyro_t gyro;
  * Gyro_Constructor(&gyro, &hi2c1);
  *
  * // 2. 初始化
  * SensorBase_Init((SensorBase_t *)&gyro);
  *
  * // 3. 周期性更新 (主循环 5Hz 门控)
  * SensorBase_Run((SensorBase_t *)&gyro);
  *
  * // 4. 读取数据
  * float yaw   = Gyro_GetYaw(&gyro);
  * float gz    = Gyro_GetGyroZ(&gyro);
  *
  * // 5. 调试打印 (通过 DebugPrintf，与 Vofa 共用 UART1)
  * Gyro_PrintInfo(&gyro, &dbg_printf);
  *
  ******************************************************************************
  */

#ifndef __GYRO_H__
#define __GYRO_H__

/* Includes ------------------------------------------------------------------*/
#include "SensorBase.h"
#include "stm32f4xx_hal.h"
#include "i2c.h"
#include "DebugPrintf.h"

/* ==================== MPU6050 寄存器地址宏 ==================== */

/**
  * @defgroup MPU6050_Registers MPU6050 寄存器地址
  * @{
  */
#define MPU6050_ADDR            (0x68 << 1)     /**< I2C 7-bit 地址左移 1 位 */
#define MPU6050_SMPLRT_DIV      0x19            /**< 采样率分频寄存器 */
#define MPU6050_CONFIG          0x1A            /**< 低通滤波配置寄存器 */
#define MPU6050_GYRO_CONFIG     0x1B            /**< 陀螺仪量程配置寄存器 */
#define MPU6050_ACCEL_CONFIG    0x1C            /**< 加速度计量程配置寄存器 */
#define MPU6050_ACCEL_XOUT_H    0x3B            /**< 加速度计 X 高字节 */
#define MPU6050_ACCEL_XOUT_L    0x3C
#define MPU6050_ACCEL_YOUT_H    0x3D
#define MPU6050_ACCEL_YOUT_L    0x3E
#define MPU6050_ACCEL_ZOUT_H    0x3F
#define MPU6050_ACCEL_ZOUT_L    0x40
#define MPU6050_TEMP_OUT_H      0x41            /**< 温度传感器高字节 */
#define MPU6050_TEMP_OUT_L      0x42            /**< 温度传感器低字节 */
#define MPU6050_GYRO_XOUT_H     0x43            /**< 陀螺仪 X 高字节 */
#define MPU6050_GYRO_XOUT_L     0x44
#define MPU6050_GYRO_YOUT_H     0x45
#define MPU6050_GYRO_YOUT_L     0x46
#define MPU6050_GYRO_ZOUT_H     0x47
#define MPU6050_GYRO_ZOUT_L     0x48
#define MPU6050_PWR_MGMT_1      0x6B            /**< 电源管理寄存器 1 */
#define MPU6050_PWR_MGMT_2      0x6C            /**< 电源管理寄存器 2 */
#define MPU6050_WHO_AM_I        0x75            /**< 设备 ID 寄存器 (默认 0x68) */
/**
  * @}
  */

/* ==================== 陀螺仪配置宏 ==================== */

/**
  * @defgroup Gyro_Config 陀螺仪配置参数
  * @{
  */
#define GYRO_UPDATE_PERIOD_MS      200U    /**< 默认更新周期 (ms)，即 5Hz */
#define GYRO_PRINT_INTERVAL_MS     500U    /**< 打印速率限制 (ms) */
#define GYRO_FS_SEL                0x08    /**< 陀螺仪满量程 ±500°/s */
#define GYRO_LSB_PER_DPS           65.5f   /**< LSB 每 °/s (500/32768*2) */
#define GYRO_ZERO_DRIFT_THRESHOLD  0.4f    /**< 零漂门限 (°/s)，低于此值视为静止 */
#define MPU6050_I2C_TIMEOUT_MS     100     /**< I2C 读写超时 (ms) */
/**
  * @}
  */

/* ==================== 陀螺仪结构体定义 ==================== */

/**
  * @brief 陀螺仪模块结构体（继承 SensorBase）
  *
  * @note   SensorBase_t 必须为第一个成员，确保指针可安全转换
  */
typedef struct Gyro_s {
    SensorBase_t       base;             /**< 基类（必须为第一个成员）*/
    I2C_HandleTypeDef  *i2c_handle;      /**< I2C 句柄指针 (hi2c1) */

    /* 原始数据 */
    int16_t            gyro_rx;          /**< 陀螺仪 X 轴原始值 */
    int16_t            gyro_ry;          /**< 陀螺仪 Y 轴原始值 */
    int16_t            gyro_rz;          /**< 陀螺仪 Z 轴原始值 */
    int16_t            temp_raw;         /**< 温度传感器原始值 */

    /* 物理量 */
    float              gyro_x;           /**< X 轴角速度 (°/s) */
    float              gyro_y;           /**< Y 轴角速度 (°/s) */
    float              gyro_z;           /**< Z 轴角速度 (°/s) */
    float              yaw;              /**< 航向角 (°)，累积陀螺仪 Z 轴积分 */
    float              temperature;      /**< 温度 (°C) */

    /* 运行状态 */
    uint8_t            flag_gyro_start;  /**< 陀螺仪首次数据就绪标志 */
    float              zero_drift_threshold; /**< 零漂门限 (°/s)，可运行时调整 */

    /* 打印速率限制 */
    uint32_t           last_print_tick;  /**< 上次打印时刻 (HAL_GetTick)，0.5s 节流 */
} Gyro_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  陀螺仪构造函数
  * @note   初始化基类成员，设置 I2C 句柄。
  *         构造后默认清零所有数据字段。
  * @param  self        指向陀螺仪对象的指针
  * @param  i2c_handle  I2C 句柄指针 (如 &hi2c1)
  */
void Gyro_Constructor(Gyro_t *self, I2C_HandleTypeDef *i2c_handle);

/**
  * @brief  获取当前航向角 (Yaw)
  * @param  self  指向陀螺仪对象的指针
  * @return float 航向角 (°)，参数无效时返回 0.0f
  */
float Gyro_GetYaw(const Gyro_t *self);

/**
  * @brief  获取 Z 轴角速度
  * @param  self  指向陀螺仪对象的指针
  * @return float Z 轴角速度 (°/s)，参数无效时返回 0.0f
  */
float Gyro_GetGyroZ(const Gyro_t *self);

/**
  * @brief  获取 X 轴角速度
  * @param  self  指向陀螺仪对象的指针
  * @return float X 轴角速度 (°/s)，参数无效时返回 0.0f
  */
float Gyro_GetGyroX(const Gyro_t *self);

/**
  * @brief  获取 Y 轴角速度
  * @param  self  指向陀螺仪对象的指针
  * @return float Y 轴角速度 (°/s)，参数无效时返回 0.0f
  */
float Gyro_GetGyroY(const Gyro_t *self);

/**
  * @brief  获取温度
  * @param  self  指向陀螺仪对象的指针
  * @return float 温度 (°C)，参数无效时返回 0.0f
  */
float Gyro_GetTemperature(const Gyro_t *self);

/**
  * @brief  查询陀螺仪是否已启动 (首次数据就绪)
  * @param  self  指向陀螺仪对象的指针
  * @return uint8_t  0 = 未启动, 1 = 已启动
  */
uint8_t Gyro_IsStarted(const Gyro_t *self);

/**
  * @brief  读取 MPU6050 设备 ID (WHO_AM_I)
  * @note   用于 I2C 总线诊断，确认设备在线。
  * @param  self  指向陀螺仪对象的指针
  * @return uint8_t 设备 ID (0x68)，读取失败返回 0x00
  */
uint8_t Gyro_GetDeviceID(Gyro_t *self);

/**
  * @brief  打印陀螺仪运动信息（通过 DebugPrintf DMA 发送）
  * @note   内置 0.5s 速率限制（per-instance），避免刷屏。
  *         单行格式示例：
  *         "[Gyro] Yaw=45.3 Gz=12.5dps T=32.1C"
  * @param  self  指向陀螺仪对象的指针（NULL 安全）
  * @param  dbg   指向 DebugPrintf 对象的指针（NULL 安全，NULL 时不输出）
  */
void Gyro_PrintInfo(Gyro_t *self, DebugPrintf_t *dbg);

#endif /* __GYRO_H__ */
