/**
  ******************************************************************************
  * @file    Gyro.c
  * @brief   陀螺仪传感器子类实现 — MPU6050 I2C DMA 读取与三轴欧拉角解算
  *
  * @details
  * 提供 Gyro_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - MPU6050 寄存器配置 (HAL I2C 阻塞写入, 仅 init 时)
  *   - I2C DMA 异步读取 (200Hz, 由 TIM2 ISR 触发)
  *   - DMA 完成回调数据处理 (HAL_I2C_MemRxCpltCallback → Gyro_DMACpltCallback)
  *   - 三轴角速度零漂门限滤波 + 欧拉角半积分
  *
  * === ISR 安全设计 ===
  *
  * Gyro_run()     — ISR 中调用, 启动 DMA 后立即返回 (< 1μs)
  * Gyro_DMACpltCallback() — I2C DMA 完成 ISR 中调用, 仅做解析+积分 (~10μs)
  *
  * === 计算公式 ===
  *
  * gyro_{x,y,z} (°/s) = raw_{x,y,z} / 32768 * 500
  * {yaw,pitch,roll}  (°) += gyro_{z,x,y} * dt_s  (经零漂门限过滤)
  *                    dt_s = 0.005 (5ms)
  *
  * 积分方式与 docs/reference/User/03BSP/Src/MPU6050_Hard.c 一致:
  *   MPU6050_PoseUpdate() 中的 yaw += gyro_z * dt 模式
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Gyro.h"
#include <stdio.h>

/* ==================== 私有宏定义 ==================== */

#define MS_TO_SEC(ms)  ((float)(ms) / 1000.0f)

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  向 MPU6050 寄存器写入一个字节 (阻塞, 仅 init 时使用)
  */
static int Gyro_WriteReg(Gyro_t *gyro, uint8_t reg, uint8_t data)
{
    if (HAL_I2C_Mem_Write(gyro->i2c_handle, MPU6050_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, &data, 1,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return 0;
}

/**
  * @brief  从 MPU6050 寄存器读取一个字节 (阻塞, 仅 init 诊断时使用)
  */
static uint8_t Gyro_ReadReg(Gyro_t *gyro, uint8_t reg)
{
    uint8_t data = 0;
    HAL_I2C_Mem_Read(gyro->i2c_handle, MPU6050_ADDR, reg,
                     I2C_MEMADD_SIZE_8BIT, &data, 1,
                     MPU6050_I2C_TIMEOUT_MS);
    return data;
}

/**
  * @brief  零漂门限滤波: 绝对值 < 门限视为静止
  */
static inline float Gyro_ApplyDeadband(float value, float threshold)
{
    if (value > -threshold && value < threshold) {
        return 0.0f;
    }
    return value;
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  陀螺仪初始化虚函数 — 配置 MPU6050 寄存器
  * @note   执行 MPU6050 上电序列:
  *          1. 唤醒设备 (PWR_MGMT_1 = 0x01)
  *          2. 复位所有传感器 (PWR_MGMT_2 = 0x00)
  *          3. 设置采样率分频 (SMPLRT_DIV = 4 → 200Hz)
  *          4. 配置低通滤波器 (CONFIG = 0x06 → 42Hz)
  *          5. 设置陀螺仪量程 (GYRO_CONFIG = 0x08 → ±500°/s)
  *          6. 设置加速度计量程 (ACCEL_CONFIG = 0x00 → ±2g)
  * @retval 0     成功
  * @retval -1    I2C 句柄无效
  * @retval -2    寄存器写入失败
  */
static int Gyro_init(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    if (gyro->i2c_handle == NULL) {
        return -1;
    }

    /* 唤醒 MPU6050 (清除睡眠位) */
    if (Gyro_WriteReg(gyro, MPU6050_PWR_MGMT_1, 0x01) != 0) {
        return -2;
    }

    /* 复位所有传感器 */
    if (Gyro_WriteReg(gyro, MPU6050_PWR_MGMT_2, 0x00) != 0) {
        return -2;
    }

    /* 采样率 = 1kHz / (1 + SMPLRT_DIV) = 200Hz */
    if (Gyro_WriteReg(gyro, MPU6050_SMPLRT_DIV, 0x04) != 0) {
        return -2;
    }

    /* 低通滤波器 42Hz (DLPF_CFG=3) */
    if (Gyro_WriteReg(gyro, MPU6050_CONFIG, 0x06) != 0) {
        return -2;
    }

    /* 陀螺仪满量程 ±500°/s */
    if (Gyro_WriteReg(gyro, MPU6050_GYRO_CONFIG, GYRO_FS_SEL) != 0) {
        return -2;
    }

    /* 加速度计满量程 ±2g */
    if (Gyro_WriteReg(gyro, MPU6050_ACCEL_CONFIG, 0x00) != 0) {
        return -2;
    }

    /* 清零所有数据 */
    gyro->gyro_rx              = 0;
    gyro->gyro_ry              = 0;
    gyro->gyro_rz              = 0;
    gyro->temp_raw             = 0;
    gyro->gyro_x               = 0.0f;
    gyro->gyro_y               = 0.0f;
    gyro->gyro_z               = 0.0f;
    gyro->yaw                  = 0.0f;
    gyro->pitch                = 0.0f;
    gyro->roll                 = 0.0f;
    gyro->temperature          = 0.0f;
    gyro->flag_gyro_start      = 0;
    gyro->zero_drift_threshold  = GYRO_ZERO_DRIFT_THRESHOLD;
    gyro->last_print_tick      = 0;
    gyro->dma_busy             = 0;

    return 0;
}

/**
  * @brief  陀螺仪运行虚函数 — 启动 I2C DMA 读取 (ISR 安全, 200Hz)
  * @note   从 TIM2 ISR (5ms 分频) 调用。
  *         启动 HAL_I2C_Mem_Read_DMA() 后立即返回 (< 1μs)。
  *         若上次 DMA 未完成则静默跳过本周期。
  *         数据处理在 Gyro_DMACpltCallback() 中异步完成。
  * @retval 0     DMA 启动成功
  * @retval -1    DMA 忙 (跳过本周期)
  * @retval -2    I2C 句柄无效
  */
static int Gyro_run(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    if (gyro->i2c_handle == NULL) {
        return -2;
    }

    /* DMA 重叠防护: 若上次传输未完成则跳过 */
    if (gyro->dma_busy) {
        return -1;
    }

    gyro->dma_busy = 1;

    /* 启动 DMA 读取 6 字节: GYRO_XOUT_H ~ GYRO_ZOUT_L */
    if (HAL_I2C_Mem_Read_DMA(gyro->i2c_handle, MPU6050_ADDR,
                              MPU6050_GYRO_XOUT_H, I2C_MEMADD_SIZE_8BIT,
                              gyro->dma_buf, GYRO_DMA_BUF_SIZE) != HAL_OK) {
        gyro->dma_busy = 0;
        return -1;
    }

    return 0;
}

/**
  * @brief  陀螺仪清理虚函数
  * @note   设置 MPU6050 进入睡眠模式，清零所有数据。
  */
static int Gyro_cleanup(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    /* 进入睡眠模式 (bit6=1) */
    if (gyro->i2c_handle != NULL) {
        Gyro_WriteReg(gyro, MPU6050_PWR_MGMT_1, 0x41);
    }

    /* 清零所有数据 */
    gyro->gyro_rx               = 0;
    gyro->gyro_ry               = 0;
    gyro->gyro_rz               = 0;
    gyro->temp_raw              = 0;
    gyro->gyro_x                = 0.0f;
    gyro->gyro_y                = 0.0f;
    gyro->gyro_z                = 0.0f;
    gyro->yaw                   = 0.0f;
    gyro->pitch                 = 0.0f;
    gyro->roll                  = 0.0f;
    gyro->temperature           = 0.0f;
    gyro->flag_gyro_start       = 0;
    gyro->zero_drift_threshold   = GYRO_ZERO_DRIFT_THRESHOLD;
    gyro->last_print_tick       = 0;
    gyro->dma_busy              = 0;

    return 0;
}

/**
  * @brief  陀螺仪复位虚函数 — 清零累积三轴欧拉角
  * @note   保留硬件配置不变，仅清零软件累积值。
  */
static void Gyro_reset(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    gyro->yaw             = 0.0f;
    gyro->pitch           = 0.0f;
    gyro->roll            = 0.0f;
    gyro->gyro_z          = 0.0f;
    gyro->gyro_x          = 0.0f;
    gyro->gyro_y          = 0.0f;
    gyro->flag_gyro_start = 0;
    gyro->last_print_tick = 0;
}

/* ==================== 子类虚函数表实例 ==================== */

static const SensorVTable_t gyro_vtable = {
    .init    = Gyro_init,
    .run     = Gyro_run,
    .cleanup = Gyro_cleanup,
    .reset   = Gyro_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  陀螺仪构造函数
  */
void Gyro_Constructor(Gyro_t *self, I2C_HandleTypeDef *i2c_handle)
{
    if (self == NULL) {
        return;
    }

    /* 1. 调用基类构造函数 */
    SensorBase_Constructor(&self->base, "Gyro");

    /* 2. 设置默认更新周期 (5ms = 200Hz) */
    self->base.update_period_ms = GYRO_UPDATE_PERIOD_MS;

    /* 3. 初始化陀螺仪特有成员 */
    self->i2c_handle             = i2c_handle;
    self->gyro_rx                = 0;
    self->gyro_ry                = 0;
    self->gyro_rz                = 0;
    self->temp_raw               = 0;
    self->gyro_x                 = 0.0f;
    self->gyro_y                 = 0.0f;
    self->gyro_z                 = 0.0f;
    self->yaw                    = 0.0f;
    self->pitch                  = 0.0f;
    self->roll                   = 0.0f;
    self->temperature            = 0.0f;
    self->flag_gyro_start        = 0;
    self->zero_drift_threshold   = GYRO_ZERO_DRIFT_THRESHOLD;
    self->last_print_tick        = 0;
    self->dma_busy               = 0;

    /* 4. 替换为子类虚函数表 */
    self->base.vtable = &gyro_vtable;
}

/**
  * @brief  I2C DMA 接收完成回调 — 解析原始数据并更新三轴欧拉角
  * @note   在 HAL_I2C_MemRxCpltCallback (ISR 上下文) 中由 Callback.c 调用。
  *         执行顺序:
  *          1. 解析 6 字节 dma_buf → 三轴原始值 (大端拼接)
  *          2. 转换为物理量 (°/s): raw / 32768 * 500
  *          3. 零漂门限滤波 (±0.4°/s)
  *          4. 三轴欧拉积分: yaw+=gz*dt, pitch+=gx*dt, roll+=gy*dt
  *         保持 ISR 快速 (~10μs, FPU 安全)
  */
void Gyro_DMACpltCallback(Gyro_t *self)
{
    /* 清除忙标志 */
    self->dma_busy = 0;

    /* 1. 解析 6 字节原始值 (大端: H << 8 | L) */
    self->gyro_rx  = (int16_t)((self->dma_buf[0] << 8) | self->dma_buf[1]);
    self->gyro_ry  = (int16_t)((self->dma_buf[2] << 8) | self->dma_buf[3]);
    self->gyro_rz  = (int16_t)((self->dma_buf[4] << 8) | self->dma_buf[5]);

    /* 2. 转换为物理量 (°/s): raw / 32768 * 500 */
    self->gyro_x = (float)self->gyro_rx / 32768.0f * 500.0f;
    self->gyro_y = (float)self->gyro_ry / 32768.0f * 500.0f;
    self->gyro_z = (float)self->gyro_rz / 32768.0f * 500.0f;

    /* 首次数据就绪后开始积分 */
    self->flag_gyro_start = 1;

    /* 3. 零漂门限滤波 */
    float gx = Gyro_ApplyDeadband(self->gyro_x, self->zero_drift_threshold);
    float gy = Gyro_ApplyDeadband(self->gyro_y, self->zero_drift_threshold);
    float gz = Gyro_ApplyDeadband(self->gyro_z, self->zero_drift_threshold);

    /* 4. 三轴欧拉积分: dt = 5ms */
    float dt_s = MS_TO_SEC(self->base.update_period_ms > 0
                           ? self->base.update_period_ms
                           : GYRO_UPDATE_PERIOD_MS);
    self->pitch += gx * dt_s;
    self->roll  += gy * dt_s;
    self->yaw   += gz * dt_s;
}

/**
  * @brief  获取当前航向角 (Yaw)
  */
float Gyro_GetYaw(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->yaw;
}

/**
  * @brief  获取当前俯仰角 (Pitch)
  */
float Gyro_GetPitch(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->pitch;
}

/**
  * @brief  获取当前横滚角 (Roll)
  */
float Gyro_GetRoll(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->roll;
}

/**
  * @brief  获取 Z 轴角速度
  */
float Gyro_GetGyroZ(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->gyro_z;
}

/**
  * @brief  获取 X 轴角速度
  */
float Gyro_GetGyroX(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->gyro_x;
}

/**
  * @brief  获取 Y 轴角速度
  */
float Gyro_GetGyroY(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->gyro_y;
}

/**
  * @brief  获取温度
  */
float Gyro_GetTemperature(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->temperature;
}

/**
  * @brief  查询陀螺仪是否已启动
  */
uint8_t Gyro_IsStarted(const Gyro_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->flag_gyro_start;
}

/**
  * @brief  读取 MPU6050 设备 ID (WHO_AM_I)
  */
uint8_t Gyro_GetDeviceID(Gyro_t *self)
{
    if (self == NULL || self->i2c_handle == NULL) {
        return 0x00;
    }
    return Gyro_ReadReg(self, MPU6050_WHO_AM_I);
}

/* ==================== 调试打印接口 ==================== */

/**
  * @brief  打印陀螺仪信息（通过 DebugPrintf DMA 发送）
  * @note   内置 0.5s 速率限制（per-instance），避免刷屏。
  *         格式: "channels:yaw,pitch,roll,gyro_z\n"
  *         符合 Vofa+ FireWater 协议多通道数据格式。
  */
void Gyro_PrintInfo(Gyro_t *self, DebugPrintf_t *dbg)
{
    if (self == NULL || dbg == NULL) {
        return;
    }

    uint32_t now = HAL_GetTick();

    /* 0.5s 速率限制 */
    if (now - self->last_print_tick < GYRO_PRINT_INTERVAL_MS) {
        return;
    }
    self->last_print_tick = now;

    DebugPrintf_Print(dbg, "channels:%.2f,%.2f,%.2f,%.2f\r\n",
                      (double)self->yaw,
                      (double)self->pitch,
                      (double)self->roll,
                      (double)self->gyro_z);
}
