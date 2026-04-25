---
name: Real Hardware Modules
description: Actual hardware modules installed in this STM32F407 project
type: reference
---

## Actual Hardware Modules in Use

This project uses the following confirmed hardware (verified by PDF datasheets in `.claude/docs/datasheets/`):

### MCU & Core
- **STM32F407VET6** - ARM Cortex-M4 @ 168 MHz with FPU
  - Datasheets: stm32f407参考手册.pdf, stm32f407数据手册.pdf, stm32f407勘误.pdf
  - I/O voltage: 3.3V
  - LQFP100 package

### Motor Drivers
1. **TB6612** (Double-channel encoder motor driver)
   - Datasheet: TB6612FNG Datasheet.pdf, 塔克创新 l TB6612双路编码器电机驱动用户手册 V1.0.pdf
   - Max output current: ~1.2A per channel
   - Logic voltage: 3.3V/5V compatible
   - Motor voltage: 4.5V-13.5V
   - Features: PWM control, direction pins, overcurrent protection
   - Use for: DC brushed motors (encoder feedback available)

2. **ULN2003** (Stepper motor driver)
   - Datasheets: ULN2003英文数据手册.pdf, ULN2003中文数据手册.pdf
   - 4 channels for stepper motor coils
   - Output current: up to 500mA per channel
   - Input: 3.3V/5V logic compatible
   - Use for: 28BYJ-48 stepper motor (4-phase control)

### Motors
1. **28BYJ-48** (Stepper Motor)
   - Datasheet: 28BYJ48规格书.doc
   - Voltage: 5V DC
   - Type: 4-phase unipolar stepper
   - Step angle: 5.625° (64 steps per revolution with gearing)
   - Use for: Precise positioning/angle control

2. **Encoded Motor (TB6612 controlled)**
   - Datasheet: 塔克创新 l MC310编码器电机用户手册 V1.0.0.pdf
   - Possible model: MC310 or similar
   - Voltage: Depends on specific model (check datasheet)
   - Use for: DC brushed motor with encoder feedback for speed/position control

### Sensors
1. **MPU6050** (6-axis IMU)
   - Datasheets: PS-MPU-6000A.pdf, RM-MPU-6000A.pdf
   - Interface: I2C (default address 0x68)
   - 3-axis accelerometer + 3-axis gyroscope + temperature
   - I/O voltage: 3.3V-5V (recommended 3.3V)
   - Schematics: MPU6050-V1.jpg, MPU6050-V1-SCH.jpg
   - Use for: Orientation/motion detection

2. **8-Channel Grayscale Sensor** (Line-following module)
   - Datasheet: 亚博智能灰度循迹模块用户入门手册.pdf
   - Other references: 循迹模块识别不良解决方案.pdf, 循迹模块数据读取.pdf, 循迹模块小车巡线.pdf
   - 8 IR sensors in array
   - Output: Analog (ADC) or digital (GPIO)
   - Voltage: 5V (or 3.3V variant)
   - Use for: Line tracking, black/white edge detection

### Power Management
1. **RT8289/RT9013** (Voltage regulators)
   - Datasheets: RT8289GSP.PDF, RT9013-33GB.PDF
   - Multiple rails for 5V → 3.3V conversion
   - Typical config: Input from USB/external, output 3.3V for MCU

### Debugging
- **Wireless DAP Debugger** - 塔克 l 无线高速DAP调试下载器手册V2.0.0.pdf
  - Supports SWD programming/debugging
  - Compatible with OpenOCD

### Additional Documentation
- **Robot Kinematics Tutorial**: 塔克创新 l 机器人运动学模型教程V1.0.4.pdf
- **IO Pin Multiplexing Guide**: IO口复用说明.png
- **Motor Compatibility Notes**: 电机兼容性说明 必看！.png
- **Button Press Warning**: 旧款关于按键按压的重要提醒(新款忽略).jpg

## Key Parameters Summary

| Component | Parameter | Value | Source |
|-----------|-----------|-------|--------|
| TB6612 | Max current | 1.2A/ch | TB6612 datasheet |
| TB6612 | Motor voltage | 4.5-13.5V | TB6612 datasheet |
| ULN2003 | Output current | 500mA/ch | ULN2003 datasheet |
| 28BYJ-48 | Step angle | 5.625° | 28BYJ48 datasheet |
| MPU6050 | I2C address | 0x68 (default) | MPU6050 datasheet |
| MPU6050 | Supply voltage | 3.3V-5V | PS-MPU-6000A.pdf |
| Grayscale | Sensor count | 8 channels | Grayscale manual |

## How to Use These Files

1. **When coding motor control**: Check TB6612 + MC310 motor datasheets for PWM frequency limits
2. **When configuring stepper**: Use ULN2003 + 28BYJ-48 datasheets for sequence timing
3. **When setting up IMU**: Reference PS-MPU-6000A.pdf for I2C register map
4. **When troubleshooting**: Check 循迹模块 PDFs for sensor calibration/issues
5. **For power design**: See RT8289/RT9013 datasheets for voltage regulation scheme
