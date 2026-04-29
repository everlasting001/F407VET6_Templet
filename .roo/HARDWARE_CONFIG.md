# 硬件配置概览

## 核心芯片

| 参数 | 值 |
|------|-----|
| MCU | STM32F407VET6 |
| 架构 | ARM Cortex-M4 with FPU |
| 主频 | 168 MHz |
| Flash | 512 KB |
| SRAM | 192 KB |

## 已连接外设

### 电机驱动
- **TB6612FNG** — 双路直流电机驱动 (I2C 或独立 PWM)
- **ULN2003** — 步进电机驱动 (28BYJ-48)

### 传感器
- **MPU6050** — 6 轴 IMU (I2C)
- **8 路灰度循迹模块** — 模拟/数字量输入

### 通信接口
- **USART1** — 调试串口 (115200 baud)
- **I2C** — MPU6050 及其他 I2C 设备

> 详细配置请参考 `.claude/docs/datasheets/` 或 `.roo/docs/datasheets/` 下的数据手册。
