# 硬件设置指南

## 调试器连接
- 使用 CMSIS-DAP 调试器（塔克无线高速 DAP）
- SWD 接口: SWCLK, SWDIO, GND
- OpenOCD 配置: `interface/cmsis-dap.cfg` + `target/stm32f4x.cfg`

## 电源
- 开发板供电: USB 5V 或外部 7-12V
- 电机供电: 独立电源 4.5-13.5V (TB6612)
- 逻辑电平: 3.3V (STM32F407)

## 外设接线

### TB6612 电机驱动
| TB6612 引脚 | STM32 引脚 |
|-------------|------------|
| PWMA/B | TIM 通道 (PWM) |
| AIN1/AIN2 | GPIO (方向控制) |
| BIN1/BIN2 | GPIO (方向控制) |
| STBY | 3.3V (使能) |

### MPU6050
| MPU6050 | STM32 引脚 |
|---------|------------|
| VCC | 3.3V |
| GND | GND |
| SCL | I2C1_SCL (PB6) |
| SDA | I2C1_SDA (PB7) |
| AD0 | GND (地址 0x68) |

> 更详细的接线图和 PCB 信息请参考 `.claude/docs/schematics/` 或 `.roo/docs/schematics/`。
