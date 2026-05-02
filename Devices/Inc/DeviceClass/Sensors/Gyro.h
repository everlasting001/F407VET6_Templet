/**
  ******************************************************************************
  * @file    Gyro.h
  * @brief   陀螺仪传感器子类 — 继承 SensorBase 的 MPU6050 六轴 IMU 实现
  *
  * @details
  * 本文件定义了陀螺仪模块的结构体和公有接口，继承自 SensorBase 基类。
  * 基类提供 name、initialized、update_period_ms 等属性，子类专注实现:
  *   - MPU6050 I2C DMA 高速读取 (200Hz)
  *   - 三轴角速度读取与零漂门限滤波 (±0.4°/s)
  *   - 欧拉角半积分累积 (Yaw/Pitch/Roll)
  *   - 角度复位与调试打印
  *
  * === 硬件配置 ===
  *
  * I2C 总线:     I2C1 (PB6=SCL, PB7=SDA)
  * 设备地址:     0x68 (7-bit)
  * 时钟频率:     100kHz
  * 陀螺仪量程:   ±500 °/s
  * 低通滤波:     42Hz (DLPF_CFG=3)
  * 采样率:       1kHz / (1+4) = 200Hz (SMPLRT_DIV=4)
  *
  * === ISR 安全 ===
  *
  * Gyro_run() 从 TIM2 ISR (5ms 分频) 调用 HAL_I2C_Mem_Read_DMA()，
  * 启动 DMA 传输后立即返回 (< 1μs)。
  * 数据处理在 I2C DMA 完成回调 (HAL_I2C_MemRxCpltCallback) 中执行，
  * 包含原始值解析、物理量转换、零漂门限、三轴欧拉积分。
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
  * // 3. ISR 触发 DMA (TIM2 5ms 分频)
  * SensorBase_Run((SensorBase_t *)&gyro);
  *
  * // 4. 读取数据
  * float yaw   = Gyro_GetYaw(&gyro);
  * float pitch = Gyro_GetPitch(&gyro);
  * float roll  = Gyro_GetRoll(&gyro);
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
#define GYRO_UPDATE_PERIOD_MS      5U      /**< 默认更新周期 (ms)，即 200Hz */
#define GYRO_PRINT_INTERVAL_MS     500U    /**< 打印速率限制 (ms) */
#define GYRO_FS_SEL                0x08    /**< 陀螺仪满量程 ±500°/s */
#define GYRO_ZERO_DRIFT_THRESHOLD  0.4f    /**< 零漂门限 (°/s)，低于此值视为静止 */
#define GYRO_DMA_BUF_SIZE          6       /**< DMA 读取字节数 (X_H/X_L/Y_H/Y_L/Z_H/Z_L) */
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

    /* DMA 传输 */
    uint8_t            dma_buf[GYRO_DMA_BUF_SIZE]; /**< I2C DMA 接收缓冲区 (6 字节) */
    volatile uint8_t   dma_busy;         /**< DMA 传输忙标志 (1=传输中, 0=空闲) */

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
    float              pitch;            /**< 俯仰角 (°)，累积陀螺仪 X 轴积分 */
    float              roll;             /**< 横滚角 (°)，累积陀螺仪 Y 轴积分 */
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
  * @brief  I2C DMA 接收完成回调 — 解析原始数据并更新三轴欧拉角
  * @note   在 HAL_I2C_MemRxCpltCallback (ISR 上下文) 中调用。
  *         执行: 解析 6 字节原始值 → 转换物理量 → 零漂门限 → 欧拉积分。
  *         保持 ISR 快速 (~10μs 纯计算，FPU 安全)。
  * @param  self  指向陀螺仪对象的指针 (调用方保证非空)
  */
void Gyro_DMACpltCallback(Gyro_t *self);

/**
  * @brief  获取当前航向角 (Yaw)
  * @param  self  指向陀螺仪对象的指针
  * @return float 航向角 (°)，参数无效时返回 0.0f
  */
float Gyro_GetYaw(const Gyro_t *self);

/**
  * @brief  获取当前俯仰角 (Pitch)
  * @param  self  指向陀螺仪对象的指针
  * @return float 俯仰角 (°)，参数无效时返回 0.0f
  */
float Gyro_GetPitch(const Gyro_t *self);

/**
  * @brief  获取当前横滚角 (Roll)
  * @param  self  指向陀螺仪对象的指针
  * @return float 横滚角 (°)，参数无效时返回 0.0f
  */
float Gyro_GetRoll(const Gyro_t *self);

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
  *         "[Gyro] Y=45.3 P=1.2 R=-0.8 Gz=12.5dps T=32.1C"
  * @param  self  指向陀螺仪对象的指针（NULL 安全）
  * @param  dbg   指向 DebugPrintf 对象的指针（NULL 安全，NULL 时不输出）
  */
void Gyro_PrintInfo(Gyro_t *self, DebugPrintf_t *dbg);

#endif /* __GYRO_H__ */
