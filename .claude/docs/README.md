# 硬件文档库索引

## 文件存储位置说明

```
.claude/docs/
├── datasheets/          # 数据手册 PDF（实际存在的文件）
│   ├── stm32f407参考手册.pdf
│   ├── stm32f407参考手册-中文版.pdf
│   ├── stm32f407数据手册.pdf
│   ├── stm32f407勘误.pdf
│   ├── PS-MPU-6000A.pdf                  # MPU6050 Register Map
│   ├── RM-MPU-6000A.pdf                  # MPU6050 Application Notes
│   ├── TB6612FNG Datasheet.pdf           # TB6612 电机驱动数据手册
│   ├── 塔克创新 l TB6612双路编码器电机驱动用户手册 V1.0.pdf
│   ├── 塔克创新 l MC310编码器电机用户手册 V1.0.0.pdf
│   ├── 28BYJ48规格书.doc
│   ├── ULN2003英文数据手册.pdf
│   ├── ULN2003中文数据手册.pdf
│   ├── 亚博智能灰度循迹模块用户入门手册.pdf
│   ├── 循迹模块数据读取.pdf
│   ├── 循迹模块识别不良解决方案.pdf
│   ├── 循迹模块小车巡线.pdf
│   ├── RT8289GSP.PDF                     # 稳压器 1
│   ├── RT9013-33GB.PDF                   # 稳压器 2
│   ├── 塔克创新 l 机器人运动学模型教程V1.0.4.pdf
│   ├── 塔克 l 无线高速DAP调试下载器手册V2.0.0.pdf
│   ├── 电机兼容性说明 必看！.png
│   ├── IO口复用说明.png
│   ├── MPU6050-V1-SCH.jpg
│   ├── MPU6050-V1.jpg
│   ├── 丝印图-反面.png
│   ├── 丝印图-正面.png
│   └── 旧款关于按键按压的重要提醒(新款忽略).jpg
│
├── schematics/          # 原理图 PDF
│   ├── FK407M3-VET6 原理图.pdf           # 主控板原理图
│   ├── FK407M3-VET6 机械尺寸.pdf
│   ├── TK-TB6612-MD220A V1.0.pdf        # TB6612 驱动板原理图
│   └── 步进电机驱动板原理图.pdf
│
└── pcb/                 # PCB 相关文件
    ├── FK407M3-VET6 原理图库.json
    └── FK407M3-VET6 封装库.json
```

## 快速查询指南

### 🔧 我想了解...

#### STM32F407 MCU 相关
| 问题 | 查看文件 | 章节/页码 |
|------|---------|---------|
| GPIO 引脚分配 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 8 |
| 中断优先级配置 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 11 |
| PWM/定时器配置 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 14-15 |
| ADC 转换 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 13 |
| UART 波特率 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 26 |
| 时钟树 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 6 |
| I2C 通信 | [`stm32f407参考手册.pdf`](datasheets/stm32f407参考手册.pdf) | Ch. 27 |

#### 电机驱动相关
| 问题 | 查看文件 | 章节/页码 |
|------|---------|---------|
| TB6612 驱动板接线 | [`TK-TB6612-MD220A V1.0.pdf`](schematics/TK-TB6612-MD220A%20V1.0.pdf) | 接线图 |
| TB6612 PWM 频率要求 | [`TB6612FNG Datasheet.pdf`](datasheets/TB6612FNG%20Datasheet.pdf) | 规格说明 |
| TB6612 最大电流限制 | [`TB6612FNG Datasheet.pdf`](datasheets/TB6612FNG%20Datasheet.pdf) | 绝对最大值 |
| MC310 编码器电机 | [`塔克创新 l MC310编码器电机用户手册 V1.0.0.pdf`](datasheets/塔克创新%20l%20MC310编码器电机用户手册%20V1.0.0.pdf) | 全篇 |
| 电机兼容性 | [`电机兼容性说明 必看！.png`](datasheets/电机兼容性说明%20必看！.png) | 图示 |

#### MPU6050 传感器
| 问题 | 查看文件 | 章节/页码 |
|------|---------|---------|
| I2C 地址和寄存器 | [`PS-MPU-6000A.pdf`](datasheets/PS-MPU-6000A.pdf) | Register Map |
| 加速度计配置 | [`RM-MPU-6000A.pdf`](datasheets/RM-MPU-6000A.pdf) | Ch. 4 |
| 陀螺仪配置 | [`RM-MPU-6000A.pdf`](datasheets/RM-MPU-6000A.pdf) | Ch. 5 |
| 中断配置 | [`RM-MPU-6000A.pdf`](datasheets/RM-MPU-6000A.pdf) | Ch. 8 |
| 温度传感器 | [`RM-MPU-6000A.pdf`](datasheets/RM-MPU-6000A.pdf) | Ch. 6 |
| 模块外观和接口 | [`MPU6050-V1-SCH.jpg`](datasheets/MPU6050-V1-SCH.jpg) | 原理图 |
| 物理照片 | [`MPU6050-V1.jpg`](datasheets/MPU6050-V1.jpg) | 实物图 |

#### 灰度循迹传感器
| 问题 | 查看文件 | 章节/页码 |
|------|---------|---------|
| 用户入门手册 | [`亚博智能灰度循迹模块用户入门手册.pdf`](datasheets/亚博智能灰度循迹模块用户入门手册.pdf) | 全篇 |
| ADC 数据读取 | [`循迹模块数据读取.pdf`](datasheets/循迹模块数据读取.pdf) | 全篇 |
| 识别不良解决方案 | [`循迹模块识别不良解决方案.pdf`](datasheets/循迹模块识别不良解决方案.pdf) | 全篇 |
| 巡线算法 | [`循迹模块小车巡线.pdf`](datasheets/循迹模块小车巡线.pdf) | 全篇 |

#### 步进电机驱动
| 问题 | 查看文件 | 章节/页码 |
|------|---------|---------|
| 28BYJ-48 规格 | [`28BYJ48规格书.doc`](datasheets/28BYJ48规格书.doc) | 规格参数 |
| ULN2003 英文手册 | [`ULN2003英文数据手册.pdf`](datasheets/ULN2003英文数据手册.pdf) | 全篇 |
| ULN2003 中文手册 | [`ULN2003中文数据手册.pdf`](datasheets/ULN2003中文数据手册.pdf) | 全篇 |
| 步进电机驱动板原理图 | [`步进电机驱动板原理图.pdf`](schematics/步进电机驱动板原理图.pdf) | 全篇 |

#### PCB 设计参考
| 问题 | 查看文件 |
|------|---------|
| FK407M3-VET6 原理图库 | [`FK407M3-VET6 原理图库.json`](pcb/FK407M3-VET6%20原理图库.json) |
| FK407M3-VET6 封装库 | [`FK407M3-VET6 封装库.json`](pcb/FK407M3-VET6%20封装库.json) |
| 主控板原理图 | [`FK407M3-VET6 原理图.pdf`](schematics/FK407M3-VET6%20原理图.pdf) |
| 主控板机械尺寸 | [`FK407M3-VET6 机械尺寸.pdf`](schematics/FK407M3-VET6%20机械尺寸.pdf) |

## 数据手册快速检索

### STM32F407VET6 参考手册

**关键参数快速查找**：

```c
// GPIO 速度等级
#define GPIO_SPEED_FREQ_LOW       // 2 MHz
#define GPIO_SPEED_FREQ_MEDIUM    // 10 MHz
#define GPIO_SPEED_FREQ_FAST      // 50 MHz  ← PWM 用此
#define GPIO_SPEED_FREQ_HIGH      // 100 MHz ← 高速应用用此

// 时钟频率
SystemCoreClock = 168 MHz  // 主时钟
```

**中断优先级（0 = 最高优先级）**：
- 0-3：硬实时任务（中断）
- 4-7：准实时任务
- 8-11：普通任务
- 12-15：后台任务

### I2C 设备地址列表

```
设备                    I2C 地址（7位）  I2C 地址（8位读）  I2C 地址（8位写）
─────────────────────────────────────────────────────────────────────────
MPU6050（默认）         0x68            0xD1             0xD0
MPU6050（AD0=1）        0x69            0xD3             0xD2
```

### MPU6050 寄存器地址速查

```c
/* 电源管理 */
0x6B → PWR_MGMT_1（电源管理）
0x6C → PWR_MGMT_2（电源管理）

/* 配置 */
0x1A → CONFIG（低通滤波器）
0x1B → GYRO_CONFIG（陀螺仪配置）
0x1C → ACCEL_CONFIG（加速度计配置）

/* 中断 */
0x37 → INT_PIN_CFG（中断引脚配置）
0x38 → INT_ENABLE（中断使能）
0x3A → INT_STATUS（中断状态）

/* 数据输出 */
0x3B-0x40 → ACCEL_XOUT_H 到 ACCEL_ZOUT_L（加速度计）
0x41-0x42 → TEMP_OUT_H/L（温度）
0x43-0x48 → GYRO_XOUT_H 到 GYRO_ZOUT_L（陀螺仪）
```

## 相关文档

- PCB 设计教程：[`tutorials/pcb/`](../tutorials/pcb/)
- 串口调试教程：[`tutorials/serial/`](../tutorials/serial/)
- 硬件配置文件：[`HARDWARE_CONFIG.md`](../HARDWARE_CONFIG.md)
- 硬件设置指南：[`HARDWARE_SETUP.md`](../HARDWARE_SETUP.md)
- 传感器集成指南：[`SENSOR_INTEGRATION.md`](../SENSOR_INTEGRATION.md)

---

> 本文档索引了 `.claude/docs/` 目录下所有实际存在的硬件文档文件。
