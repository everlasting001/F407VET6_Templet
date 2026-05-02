/**
  ******************************************************************************
  * @file    Gyro.c
  * @brief   陀螺仪传感器子类实现 — MPU6050 I2C 读写与航向角解算
  *
  * @details
  * 提供 Gyro_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - MPU6050 寄存器读写 (HAL I2C)
  *   - Z 轴角速度读取与零漂门限滤波
  *   - 航向角 (Yaw) 半积分累积
  *   - 角度归一化与硬件复位
  *
  * === ISR 安全说明 ===
  *
  * SensorBase_Run() 内部调用 HAL I2C 阻塞读取 (~200μs)，
  * 不可在 TIM ISR 中调用。必须在主循环 while(1) 中以门控
  * 周期调用（推荐 5Hz/200ms）。
  *
  * === 计算公式 ===
  *
  * gyro_z  (°/s) = raw_z / 32768 * 500
  * yaw     (°)   += gyro_z * dt_s  (经零漂门限过滤后)
  * temp    (°C)  = raw_temp / 340 + 36.53
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Gyro.h"
#include <stdio.h>
#include <math.h>

/* ==================== 私有宏定义 ==================== */

#define MS_TO_SEC(ms)  ((float)(ms) / 1000.0f)

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  向 MPU6050 寄存器写入一个字节
  * @param  gyro  指向陀螺仪对象的指针 (调用方保证非空)
  * @param  reg   寄存器地址
  * @param  data  写入数据
  * @retval 0     成功
  * @retval -1    I2C 写入失败
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
  * @brief  从 MPU6050 寄存器读取一个字节
  * @param  gyro  指向陀螺仪对象的指针 (调用方保证非空)
  * @param  reg   寄存器地址
  * @return uint8_t 读取值，失败返回 0x00
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
  * @brief  从 MPU6050 连续读取多个寄存器
  * @param  gyro  指向陀螺仪对象的指针 (调用方保证非空)
  * @param  reg   起始寄存器地址
  * @param  buf   输出缓冲区
  * @param  len   读取字节数
  * @retval 0     成功
  * @retval -1    I2C 读取失败
  */
static int Gyro_ReadMulti(Gyro_t *gyro, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (HAL_I2C_Mem_Read(gyro->i2c_handle, MPU6050_ADDR, reg,
                         I2C_MEMADD_SIZE_8BIT, buf, len,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }
    return 0;
}

/**
  * @brief  角度归一化到 [-180, 180)
  * @param  angle  原始角度 (°)
  * @return float  归一化后的角度 (°)
  */
static float Gyro_NormalizeAngle(float angle)
{
    angle = fmodf(angle + 180.0f, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    if (angle >= 180.0f) {
        angle -= 360.0f;
    }
    return angle;
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  陀螺仪初始化虚函数 — 配置 MPU6050 寄存器
  * @note   执行 MPU6050 上电序列:
  *          1. 唤醒设备 (PWR_MGMT_1 = 0x01)
  *          2. 复位所有传感器 (PWR_MGMT_2 = 0x00)
  *          3. 设置采样率分频 (SMPLRT_DIV = 9 → 100Hz)
  *          4. 配置低通滤波器 (CONFIG = 0x06 → 42Hz)
  *          5. 设置陀螺仪量程 (GYRO_CONFIG = 0x08 → ±500°/s)
  *          6. 设置加速度计量程 (ACCEL_CONFIG = 0x00 → ±2g)
  * @param  self  指向模块对象自身的 void 指针
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

    /* 采样率 = 1kHz / (1 + SMPLRT_DIV) = 100Hz */
    if (Gyro_WriteReg(gyro, MPU6050_SMPLRT_DIV, 0x09) != 0) {
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
    gyro->gyro_rx             = 0;
    gyro->gyro_ry             = 0;
    gyro->gyro_rz             = 0;
    gyro->temp_raw            = 0;
    gyro->gyro_x              = 0.0f;
    gyro->gyro_y              = 0.0f;
    gyro->gyro_z              = 0.0f;
    gyro->yaw                 = 0.0f;
    gyro->temperature         = 0.0f;
    gyro->flag_gyro_start     = 0;
    gyro->zero_drift_threshold = GYRO_ZERO_DRIFT_THRESHOLD;
    gyro->last_print_tick     = 0;

    return 0;
}

/**
  * @brief  陀螺仪运行虚函数 — 读取数据并更新航向角
  * @note   在 main() while(1) 中按 update_period_ms 门控调用。
  *         不可在 ISR 中调用 —— 使用了阻塞 HAL I2C API。
  *
  *         执行顺序:
  *          1. 读取陀螺仪 Z 轴原始数据
  *          2. 读取温度原始数据
  *          3. 转换为物理量 (°/s, °C)
  *          4. 零漂门限滤波
  *          5. 半积分累积航向角
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    I2C 读取失败
  * @retval -2    I2C 句柄无效
  */
static int Gyro_run(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    if (gyro->i2c_handle == NULL) {
        return -2;
    }

    /* 读取陀螺仪 Z 轴数据 (GYRO_ZOUT_H, GYRO_ZOUT_L) 和温度 */
    uint8_t buf[4];
    if (Gyro_ReadMulti(gyro, MPU6050_GYRO_ZOUT_H, buf, 4) != 0) {
        return -1;
    }

    /* 原始值拼接 (大端: H << 8 | L) */
    gyro->gyro_rz  = (int16_t)((buf[0] << 8) | buf[1]);
    gyro->temp_raw = (int16_t)((buf[2] << 8) | buf[3]);

    /* 转换为物理量 */
    /* gyro_z: raw / 32768 * 500°/s */
    gyro->gyro_z = (float)gyro->gyro_rz / 32768.0f * 500.0f;

    /* 温度: raw / 340 + 36.53 (°C) */
    gyro->temperature = (float)gyro->temp_raw / 340.0f + 36.53f;

    /* 首次数据就绪后开始积分 */
    gyro->flag_gyro_start = 1;

    /* 零漂门限滤波: 绝对值 < 门限视为静止 */
    float gz = gyro->gyro_z;
    if (gz > -gyro->zero_drift_threshold && gz < gyro->zero_drift_threshold) {
        gz = 0.0f;
    }

    /* 半积分累积航向角 */
    float dt_s = MS_TO_SEC(gyro->base.update_period_ms > 0
                           ? gyro->base.update_period_ms
                           : GYRO_UPDATE_PERIOD_MS);
    gyro->yaw += gz * dt_s;

    return 0;
}

/**
  * @brief  陀螺仪清理虚函数
  * @note   设置 MPU6050 进入睡眠模式，清零所有数据。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int Gyro_cleanup(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    /* 进入睡眠模式 (bit6=1) */
    if (gyro->i2c_handle != NULL) {
        Gyro_WriteReg(gyro, MPU6050_PWR_MGMT_1, 0x41);
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
    gyro->temperature          = 0.0f;
    gyro->flag_gyro_start      = 0;
    gyro->zero_drift_threshold  = GYRO_ZERO_DRIFT_THRESHOLD;
    gyro->last_print_tick      = 0;

    return 0;
}

/**
  * @brief  陀螺仪复位虚函数 — 清零累积航向角
  * @note   保留硬件配置不变，仅清零软件累积值。
  * @param  self  指向模块对象自身的 void 指针
  */
static void Gyro_reset(void *self)
{
    Gyro_t *gyro = (Gyro_t *)self;

    gyro->yaw             = 0.0f;
    gyro->gyro_z          = 0.0f;
    gyro->gyro_x          = 0.0f;
    gyro->gyro_y          = 0.0f;
    gyro->flag_gyro_start = 0;
    gyro->last_print_tick = 0;
}

/* ==================== 子类虚函数表实例 ==================== */

/**
  * @brief 陀螺仪模块虚函数表
  * @note  所有虚函数均被重写，提供完整的 MPU6050 行为。
  */
static const SensorVTable_t gyro_vtable = {
    .init    = Gyro_init,
    .run     = Gyro_run,
    .cleanup = Gyro_cleanup,
    .reset   = Gyro_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  陀螺仪构造函数
  * @param  self        指向陀螺仪对象的指针
  * @param  i2c_handle  I2C 句柄指针 (如 &hi2c1)
  */
void Gyro_Constructor(Gyro_t *self, I2C_HandleTypeDef *i2c_handle)
{
    if (self == NULL) {
        return;
    }

    /* 1. 调用基类构造函数 */
    SensorBase_Constructor(&self->base, "Gyro");

    /* 2. 设置默认更新周期 */
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
    self->temperature            = 0.0f;
    self->flag_gyro_start        = 0;
    self->zero_drift_threshold   = GYRO_ZERO_DRIFT_THRESHOLD;
    self->last_print_tick        = 0;

    /* 4. 替换为子类虚函数表 */
    self->base.vtable = &gyro_vtable;
}

/**
  * @brief  获取当前航向角 (Yaw)
  * @param  self  指向陀螺仪对象的指针
  * @return float 航向角 (°)
  */
float Gyro_GetYaw(const Gyro_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->yaw;
}

/**
  * @brief  获取 Z 轴角速度
  * @param  self  指向陀螺仪对象的指针
  * @return float Z 轴角速度 (°/s)
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
  * @param  self  指向陀螺仪对象的指针
  * @return float X 轴角速度 (°/s)
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
  * @param  self  指向陀螺仪对象的指针
  * @return float Y 轴角速度 (°/s)
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
  * @param  self  指向陀螺仪对象的指针
  * @return float 温度 (°C)
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
  * @param  self  指向陀螺仪对象的指针
  * @return uint8_t  0 = 未启动, 1 = 已启动
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
  * @note   用于 I2C 总线诊断，确认设备在线。
  *         正常返回值应为 0x68。
  * @param  self  指向陀螺仪对象的指针
  * @return uint8_t 设备 ID，读取失败返回 0x00
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
  *         若距离上次成功打印不足 500ms，则静默跳过。
  *         self 或 dbg 为 NULL 时静默返回。
  * @param  self  指向陀螺仪对象的指针
  * @param  dbg   指向 DebugPrintf 对象的指针
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

    /* 格式化并发送，格式: "[Gyro] Yaw=45.3 Gz=12.5dps T=32.1C" */
    DebugPrintf_Print(dbg, "[Gyro] Yaw=%.1f Gz=%.1fdps T=%.1fC\r\n",
                      (double)self->yaw,
                      (double)self->gyro_z,
                      (double)self->temperature);
}
