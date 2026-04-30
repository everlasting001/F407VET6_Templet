# 电机配置

## 直流编码器电机（Motor_R 与 Motor_L）

### 硬件信息
- **电机类型**: 直流有刷编码器电机
- **驱动芯片**: TB6612FNG
- **驱动板型号**: TK-TB6612-MD220A
- **编码器型号**: MC310

### 相关文档
- TB6612 数据手册: `.claude/docs/datasheets/TB6612FNG Datasheet.pdf`
- TB6612 中文手册: `.claude/docs/datasheets/塔克创新 l TB6612双路编码器电机驱动用户手册 V1.0.pdf`
- MC310 编码器电机手册: `.claude/docs/datasheets/塔克创新 l MC310编码器电机用户手册 V1.0.0.pdf`
- 驱动板原理图: `.claude/docs/schematics/TK-TB6612-MD220A V1.0.pdf`
- 电机兼容性说明: `.claude/docs/datasheets/电机兼容性说明 必看！.png`

### 控制接口
| 功能 | 引脚 | 说明 |
|------|------|------|
| Motor_R PWM | TIMx_CHy | 右电机速度控制 |
| Motor_R DIR1 | GPIO | 右电机方向1 |
| Motor_R DIR2 | GPIO | 右电机方向2 |
| Motor_L PWM | TIMx_CHy | 左电机速度控制 |
| Motor_L DIR1 | GPIO | 左电机方向1 |
| Motor_L DIR2 | GPIO | 左电机方向2 |

### 控制逻辑（TB6612）
| 模式 | AIN1 | AIN2 | PWM |
|------|------|------|-----|
| 正转 | 1 | 0 | PWM |
| 反转 | 0 | 1 | PWM |
| 制动 | 1 | 1 | X |
| 停止 | 0 | 0 | X |

> **注意**: STBY 引脚必须拉高才能使能驱动。
> PWM 频率建议 10-50kHz。
