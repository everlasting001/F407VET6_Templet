---
name: Quick I2C Reference
description: Fast lookup for I2C addresses and critical MPU6050 registers
type: reference
---

## I2C Device Addresses

| Device | 7-bit Address | 8-bit Read | 8-bit Write | Notes |
|--------|---------------|-----------|-----------|-------|
| MPU6050 (default) | 0x68 | 0xD1 | 0xD0 | AD0 pin low |
| MPU6050 (alt) | 0x69 | 0xD3 | 0xD2 | AD0 pin high |

**Data source**: PS-MPU-6000A.pdf (MPU6050 Register Map)

## MPU6050 Critical Registers (Quick Lookup)

```c
// Power Management (必须首先初始化)
#define PWR_MGMT_1      0x6B  // 写 0x00 唤醒
#define PWR_MGMT_2      0x6C

// Configuration
#define CONFIG          0x1A  // DLPF (Low-Pass Filter)
#define GYRO_CONFIG     0x1B  // 量程选择
#define ACCEL_CONFIG    0x1C  // 加速度量程选择

// Interrupt
#define INT_PIN_CFG     0x37
#define INT_ENABLE      0x38
#define INT_STATUS      0x3A

// Data Output (6 bytes accelerometer, 2 bytes temp, 6 bytes gyro)
#define ACCEL_XOUT_H    0x3B  // 加速度 X 高字节 (开始地址)
#define ACCEL_XOUT_L    0x3C  // 加速度 X 低字节
#define ACCEL_YOUT_H    0x3D
#define ACCEL_YOUT_L    0x3E
#define ACCEL_ZOUT_H    0x3F
#define ACCEL_ZOUT_L    0x40
#define TEMP_OUT_H      0x41
#define TEMP_OUT_L      0x42
#define GYRO_XOUT_H     0x43  // 陀螺仪 X 高字节 (开始地址)
#define GYRO_XOUT_L     0x44
#define GYRO_YOUT_H     0x45
#define GYRO_YOUT_L     0x46
#define GYRO_ZOUT_H     0x47
#define GYRO_ZOUT_L     0x48

// WHO_AM_I (验证连接)
#define WHO_AM_I        0x75  // 应返回 0x68
```

## MPU6050 Initialization Sequence

1. **Write to PWR_MGMT_1 (0x6B)**: 0x00 (清除睡眠位，启用时钟)
2. **Wait**: 100ms stabilization
3. **Read WHO_AM_I (0x75)**: Should return 0x68
4. **Configure GYRO_CONFIG (0x1B)**: Set gyro range (0x00 = ±250°/s, etc.)
5. **Configure ACCEL_CONFIG (0x1C)**: Set accel range (0x00 = ±2g, etc.)
6. **Optionally configure CONFIG (0x1A)**: Set DLPF bandwidth
7. **Enable interrupt (0x38)**: If using interrupt mode

## Data Readout

**Easiest approach**: Read 14 bytes starting from ACCEL_XOUT_H (0x3B):
- Bytes 0-1: ACCEL_X (int16_t, big-endian)
- Bytes 2-3: ACCEL_Y
- Bytes 4-5: ACCEL_Z
- Bytes 6-7: TEMP
- Bytes 8-9: GYRO_X
- Bytes 10-11: GYRO_Y
- Bytes 12-13: GYRO_Z

**Code snippet**:
```c
uint8_t buffer[14];
HAL_I2C_Mem_Read(&hi2c1, 0x68 << 1, 0x3B, I2C_MEMADD_SIZE_8BIT, buffer, 14, HAL_MAX_DELAY);

int16_t accel_x = (int16_t)(buffer[0] << 8 | buffer[1]);
int16_t accel_y = (int16_t)(buffer[2] << 8 | buffer[3]);
int16_t accel_z = (int16_t)(buffer[4] << 8 | buffer[5]);
int16_t temp    = (int16_t)(buffer[6] << 8 | buffer[7]);
int16_t gyro_x  = (int16_t)(buffer[8] << 8 | buffer[9]);
int16_t gyro_y  = (int16_t)(buffer[10] << 8 | buffer[11]);
int16_t gyro_z  = (int16_t)(buffer[12] << 8 | buffer[13]);
```

## Configuration Values (Common Setups)

### Gyro Range (GYRO_CONFIG 0x1B, bits 3-4)
- 0x00: ±250°/s (most sensitive, default)
- 0x08: ±500°/s
- 0x10: ±1000°/s
- 0x18: ±2000°/s (least sensitive)

### Accelerometer Range (ACCEL_CONFIG 0x1C, bits 3-4)
- 0x00: ±2g (most sensitive, default)
- 0x08: ±4g
- 0x10: ±8g
- 0x18: ±16g (least sensitive)

### DLPF (CONFIG 0x1A, bits 2-0)
- 0x00: 260 Hz (no DLPF)
- 0x01: 184 Hz
- 0x02: 94 Hz (good for 100 Hz sampling)
- 0x03: 44 Hz
- 0x04: 21 Hz
- 0x05: 10 Hz
- 0x06: 5 Hz

**Recommendation for 100 Hz main loop**: Set DLPF to 0x02 (94 Hz) to filter high-frequency noise

## I2C STM32 Integration (STM32F407)

**Typical I2C3 Configuration**:
- SCL: PB8
- SDA: PB9
- Pull-up resistors: 4.7 kΩ (usually on eval boards)
- Frequency: 100 kHz (standard) or 400 kHz (fast)

**STM32CubeMX settings**:
1. Enable I2C3
2. Set clock speed to 100 kHz or 400 kHz
3. Configure GPIO: PB8/PB9 as I2C3
4. Enable global interrupt if needed

**Data source**: stm32f407参考手册.pdf (I2C章节)
