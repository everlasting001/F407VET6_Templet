# 传感器配置

## MPU6050 (6 轴 IMU)

### 硬件信息
- **传感器型号**: MPU6050
- **接口**: I2C
- **I2C 地址**: 0x68 (AD0=GND) / 0x69 (AD0=VCC)

### 相关文档
- 寄存器映射: `.claude/docs/datasheets/PS-MPU-6000A.pdf`
- 应用笔记: `.claude/docs/datasheets/RM-MPU-6000A.pdf`
- 模块原理图: `.claude/docs/datasheets/MPU6050-V1-SCH.jpg`
- 模块实物图: `.claude/docs/datasheets/MPU6050-V1.jpg`

### 关键寄存器
| 寄存器 | 地址 | 描述 |
|--------|------|------|
| PWR_MGMT_1 | 0x6B | 电源管理，需写 0x00 唤醒 |
| CONFIG | 0x1A | 数字低通滤波器配置 |
| GYRO_CONFIG | 0x1B | 陀螺仪量程配置 |
| ACCEL_CONFIG | 0x1C | 加速度计量程配置 |
| ACCEL_XOUT_H | 0x3B | 加速度计 X 轴高字节 |
| GYRO_XOUT_H | 0x43 | 陀螺仪 X 轴高字节 |
| WHO_AM_I | 0x75 | 设备 ID（应返回 0x68） |

## 8 路灰度循迹模块

### 硬件信息
- **传感器类型**: 模拟输出
- **工作电压**: 5V
- **检测距离**: 1-3 cm
- **通道数**: 8 路

### 相关文档
- 用户手册: `.claude/docs/datasheets/亚博智能灰度循迹模块用户入门手册.pdf`
- 数据读取: `.claude/docs/datasheets/循迹模块数据读取.pdf`
- 问题排查: `.claude/docs/datasheets/循迹模块识别不良解决方案.pdf`
- 巡线算法: `.claude/docs/datasheets/循迹模块小车巡线.pdf`

### ADC 配置建议
- ADC 分辨率: 12-bit (0-4095)
- 白底阈值: < 512
- 黑线阈值: > 2048
- 或使用动态阈值: `(max + min) / 2`

## 步进电机 28BYJ-48

### 硬件信息
- **电机型号**: 28BYJ-48
- **驱动芯片**: ULN2003
- **步进模式**: 4 相 5 线 unipolar
- **减速比**: 1:64 (实际 2048 步/圈)

### 相关文档
- 规格书: `.claude/docs/datasheets/28BYJ48规格书.doc`
- ULN2003 英文手册: `.claude/docs/datasheets/ULN2003英文数据手册.pdf`
- ULN2003 中文手册: `.claude/docs/datasheets/ULN2003中文数据手册.pdf`
- 驱动板原理图: `.claude/docs/schematics/步进电机驱动板原理图.pdf`

### 8 步控制时序
| 步进 | A | B | C | D |
|------|---|---|---|---|
| 1 | 1 | 0 | 0 | 0 |
| 2 | 1 | 1 | 0 | 0 |
| 3 | 0 | 1 | 0 | 0 |
| 4 | 0 | 1 | 1 | 0 |
| 5 | 0 | 0 | 1 | 0 |
| 6 | 0 | 0 | 1 | 1 |
| 7 | 0 | 0 | 0 | 1 |
| 8 | 1 | 0 | 0 | 1 |

> 步进延时调节速度: 2-20ms 典型范围。
> 全步模式: 2048 步/圈，半步模式: 4096 步/圈。
