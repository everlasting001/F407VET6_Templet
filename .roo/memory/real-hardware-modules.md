# 实际硬件模块清单

> 此文件是 `.claude/memory/real-hardware-modules.md` 的副本，供 Roo Code 使用。

## 已确认硬件

| # | 模块 | 型号 | 接口 | 数据手册 |
|---|------|------|------|---------|
| 1 | 电机驱动 | TB6612FNG | PWM + 方向 | `TB6612FNG Datasheet.pdf` |
| 2 | 6 轴 IMU | MPU6050 | I2C | `PS-MPU-6000A.pdf` |
| 3 | 灰度循迹 | 8 通道 | ADC / GPIO | `亚博智能灰度循迹模块用户入门手册.pdf` |
| 4 | 步进电机 | 28BYJ-48 | 4 相 GPIO | `28BYJ48规格书.doc` |
| 5 | 步进驱动 | ULN2003 | 4 相 GPIO | `ULN2003英文数据手册.pdf` |

## 文档位置
所有数据手册、原理图等均在 `.claude/docs/` 目录下。
