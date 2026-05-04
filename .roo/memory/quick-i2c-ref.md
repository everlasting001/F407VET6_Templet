# MPU6050 I2C 快速参考

> 此文件是 `.claude/memory/quick-i2c-ref.md` 的副本，供 Roo Code 使用。

## 设备信息
- **地址**: 0x68 (AD0=GND) / 0x69 (AD0=VCC)
- **通信**: I2C, 标准模式 100kHz / 快速模式 400kHz

## 关键寄存器

| 地址 | 名称 | 描述 | 默认值 |
|------|------|------|--------|
| 0x6B | PWR_MGMT_1 | 电源管理 | 0x40 |
| 0x19 | SMPLRT_DIV | 采样率分频 | 0x00 |
| 0x1A | CONFIG | 数字低通滤波 | 0x00 |
| 0x1B | GYRO_CONFIG | 陀螺仪配置 | 0x00 |
| 0x1C | ACCEL_CONFIG | 加速度计配置 | 0x00 |
| 0x75 | WHO_AM_I | 设备 ID | 0x68 |

## 初始化序列
1. PWR_MGMT_1 (0x6B) ← 0x00 (唤醒)
2. SMPLRT_DIV (0x19) ← 0x07 (采样率 1kHz)
3. CONFIG (0x1A) ← 0x00 (DLPF 260Hz)
4. GYRO_CONFIG (0x1B) ← 0x00 (±250°/s)
5. ACCEL_CONFIG (0x1C) ← 0x00 (±2g)
