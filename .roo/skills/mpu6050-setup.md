# Skill: MPU6050 初始化与数据读取

## 用途
为 STM32F407 项目配置 MPU6050 6 轴 IMU 传感器，包括 I2C 初始化、传感器校准和数据读取。

## 使用方式
在 Roo Code 中通过 `skill("mpu6050-setup", "<动作>")` 调用。

## 初始化流程

### 1. I2C 初始化
确保 CubeMX 中已配置 I2C1 (PB6-SCL, PB7-SDA)，速度 400kHz。

### 2. MPU6050 寄存器配置
```c
// 唤醒设备
uint8_t pwr_mgmt = 0x00;  // 清除 SLEEP 位
HAL_I2C_Mem_Write(&hi2c1, 0x68<<1, 0x6B, I2C_MEMADD_SIZE_8BIT, &pwr_mgmt, 1, 100);

// 配置采样率
uint8_t smplrt_div = 0x07;  // 1kHz 采样率
HAL_I2C_Mem_Write(&hi2c1, 0x68<<1, 0x19, I2C_MEMADD_SIZE_8BIT, &smplrt_div, 1, 100);

// 配置 DLPF
uint8_t config = 0x00;  // 260Hz 带宽
HAL_I2C_Mem_Write(&hi2c1, 0x68<<1, 0x1A, I2C_MEMADD_SIZE_8BIT, &config, 1, 100);

// 配置陀螺仪量程 (±250°/s)
uint8_t gyro_config = 0x00;
HAL_I2C_Mem_Write(&hi2c1, 0x68<<1, 0x1B, I2C_MEMADD_SIZE_8BIT, &gyro_config, 1, 100);

// 配置加速度计量程 (±2g)
uint8_t accel_config = 0x00;
HAL_I2C_Mem_Write(&hi2c1, 0x68<<1, 0x1C, I2C_MEMADD_SIZE_8BIT, &accel_config, 1, 100);
```

### 3. 数据读取
```c
uint8_t raw_data[14];
int16_t accel_x, accel_y, accel_z, temp, gyro_x, gyro_y, gyro_z;

HAL_I2C_Mem_Read(&hi2c1, 0x68<<1, 0x3B, I2C_MEMADD_SIZE_8BIT, raw_data, 14, 100);

accel_x = (raw_data[0] << 8) | raw_data[1];
accel_y = (raw_data[2] << 8) | raw_data[3];
accel_z = (raw_data[4] << 8) | raw_data[5];
temp    = (raw_data[6] << 8) | raw_data[7];
gyro_x  = (raw_data[8] << 8) | raw_data[9];
gyro_y  = (raw_data[10] << 8) | raw_data[11];
gyro_z  = (raw_data[12] << 8) | raw_data[13];
```

## 校准
- 静止时采样 100 次取平均作为偏移量
- 陀螺仪偏移：静止时各轴读数
- 加速度计偏移：Z 轴应减去 1g
