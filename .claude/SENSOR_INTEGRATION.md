# 多传感器模块快速集成指南

## 项目硬件配置

```
STM32F407VET6 (核心)
├─ 🤖 DC 有刷电机 (H桥驱动)           → .claude/rules/motor-control-guide.md
├─ 📡 MPU6050 (6轴 IMU)              → .claude/rules/sensor-modules-guide.md
├─ 👀 8路灰度循迹模块                  → .claude/rules/sensor-modules-guide.md
├─ ⚙️ 步进电机驱动 (ULN2003)          → .claude/rules/sensor-modules-guide.md
└─ 🔋 电源管理                         → .claude/rules/hardware-integration.md
```

## 文件位置清单

### 数据手册和原理图

**MPU6050**:
```
.claude/docs/datasheets/PS-MPU-6000A.pdf          ← Register Map
.claude/docs/datasheets/RM-MPU-6000A.pdf          ← Application Notes
.claude/docs/datasheets/MPU6050-V1-SCH.jpg         ← 模块原理图
```

**灰度循迹模块**:
```
.claude/docs/datasheets/亚博智能灰度循迹模块用户入门手册.pdf
.claude/docs/datasheets/循迹模块数据读取.pdf
.claude/docs/datasheets/循迹模块识别不良解决方案.pdf
.claude/docs/datasheets/循迹模块小车巡线.pdf
```

**步进电机驱动**:
```
.claude/docs/datasheets/28BYJ48规格书.doc
.claude/docs/datasheets/ULN2003英文数据手册.pdf
.claude/docs/datasheets/ULN2003中文数据手册.pdf
.claude/docs/schematics/步进电机驱动板原理图.pdf
```

**直流电机驱动**:
```
.claude/docs/datasheets/TB6612FNG Datasheet.pdf
.claude/docs/datasheets/塔克创新 l TB6612双路编码器电机驱动用户手册 V1.0.pdf
.claude/docs/datasheets/塔克创新 l MC310编码器电机用户手册 V1.0.0.pdf
.claude/docs/schematics/TK-TB6612-MD220A V1.0.pdf
```

## 快速 STM32CubeMX 配置

### I2C (MPU6050)

**在 STM32CubeMX 中**：
1. 选择 Connectivity → I2C3
2. 模式：I2C
3. GPIO 设置：
   - SCL: PB8
   - SDA: PB9
4. 时钟：400 kHz（标准模式）或 1 MHz（快速模式）
5. 中断：启用 I2C3_EV_IRQn 和 I2C3_ER_IRQn

**生成后在 main.c 中**：
```c
/* 在 USER CODE BEGIN 2 */
mpu6050_init();  // 初始化
/* USER CODE END 2 */

/* 在主循环中 */
MPU6050_Data_t data;
mpu6050_read(&data);
printf("Accel: %d, %d, %d\r\n", data.accel_x, data.accel_y, data.accel_z);
```

### ADC + DMA (灰度传感器)

**在 STM32CubeMX 中**：
1. 选择 Analog → ADC1
2. 通道设置：
   - 启用 IN0-IN7 (PA0-PA7)
   - 设置数据对齐：Right aligned
   - 采样时间：168 cycles（长采样时间提高精度）
3. DMA 设置：
   - 启用 DMA2 Stream 0
   - 模式：Circular（连续转换）
   - 数据宽度：Word-to-Word

**生成后在 main.c 中**：
```c
/* USER CODE BEGIN 2 */
HAL_ADC_Start_DMA(&hadc1, (uint32_t *)grayscale_data.sensor, 8);
/* USER CODE END 2 */

/* 在主循环中 */
grayscale_read(&grayscale_data);
printf("Grayscale position: %d\r\n", grayscale_data.position);
```

### GPIO (步进电机)

**在 STM32CubeMX 中**：
1. 选择 GPIO
2. 配置 4 个输出引脚：PA8, PA9, PA10, PA11
3. GPIO 模式：Output Push-Pull
4. 速度：High

**生成后在 main.c 中**：
```c
/* USER CODE BEGIN 2 */
Stepper_t stepper;
stepper_init(&stepper);
/* USER CODE END 2 */

/* 在主循环中 */
stepper_rotate_steps(&stepper, 100, 5);  // 旋转 100 步，每步 5ms
```

## 针脚冲突检查清单

```
┌─────────────────────────────────────────────────────┐
│ 确保没有针脚重复使用！                               │
├─────────────────────────────────────────────────────┤
│ DC 电机:         PA0 (PWM), PA1-2 (GPIO)           │
│ 步进电机:        PA8-11 (GPIO)                     │
│ 灰度传感器:      PA3-PA7 + 更多 ADC (DMA)         │
│ MPU6050:         PB8 (SCL), PB9 (SDA)             │
│ UART (调试):     PA9 (TX), PA10 (RX)              │
└─────────────────────────────────────────────────────┘
```

## 中断优先级分配

```c
/* 在 main.c 中配置，在 SystemClock_Config() 之后 */

/* I2C (MPU6050): 优先级 1 */
HAL_NVIC_SetPriority(I2C3_EV_IRQn, 1, 0);
HAL_NVIC_SetPriority(I2C3_ER_IRQn, 1, 1);
HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
HAL_NVIC_EnableIRQ(I2C3_ER_IRQn);

/* DMA (灰度传感器): 优先级 2 */
HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

/* UART (调试): 优先级 3 */
HAL_NVIC_SetPriority(USART1_IRQn, 3, 0);
HAL_NVIC_EnableIRQ(USART1_IRQn);

/* 定时器 (步进电机定时): 优先级 4 */
HAL_NVIC_SetPriority(TIM2_IRQn, 4, 0);
HAL_NVIC_EnableIRQ(TIM2_IRQn);
```

## 完整的 main.c 示例框架

```c
/* USER CODE BEGIN Includes */
#include "app_motor.h"
#include "app_mpu6050.h"
#include "app_grayscale.h"
#include "app_stepper.h"
/* USER CODE END Includes */

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C3_Init();
    MX_ADC1_Init();
    MX_UART1_Init();
    MX_TIM2_Init();

    /* USER CODE BEGIN 2 */
    // 初始化所有模块
    motor_init();
    mpu6050_init();
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)grayscale_data.sensor, 8);
    Stepper_t stepper;
    stepper_init(&stepper);
    
    printf("All modules initialized\r\n");
    /* USER CODE END 2 */

    while (1) {
        /* USER CODE BEGIN 3 */
        
        // 1. 读取 MPU6050 数据
        MPU6050_Data_t mpu_data;
        mpu6050_read(&mpu_data);
        
        // 2. 读取灰度传感器
        grayscale_read(&grayscale_data);
        
        // 3. 基于灰度传感器位置控制 DC 电机
        if (grayscale_data.position < -10) {
            motor_set_direction(2);  // 反转
            motor_set_speed(50);
        } else if (grayscale_data.position > 10) {
            motor_set_direction(1);  // 正转
            motor_set_speed(50);
        } else {
            motor_set_direction(1);
            motor_set_speed(100);    // 直行
        }
        
        // 4. 基于加速度控制步进电机
        if (mpu_data.accel_z > 15000) {
            stepper_rotate_steps(&stepper, 10, 5);
        }
        
        // 5. 输出调试信息
        printf("Accel: %d, %d, %d | Grayscale: %d | Stepper pos: %ld\r\n",
               mpu_data.accel_x, mpu_data.accel_y, mpu_data.accel_z,
               grayscale_data.position, stepper.position);
        
        HAL_Delay(100);
        
        /* USER CODE END 3 */
    }
}
```

## 调试技巧

### 1. I2C 通信问题诊断

```c
/* 在 main.c 的初始化部分添加 */
uint8_t mpu_id;
if (HAL_I2C_Mem_Read(&hi2c3, 0x68 << 1, 0x75, 1, &mpu_id, 1, 100) == HAL_OK) {
    printf("MPU6050 ID: 0x%02X (should be 0x68)\r\n", mpu_id);
} else {
    printf("ERROR: MPU6050 not responding!\r\n");
}
```

### 2. ADC 读值异常

```c
/* 检查原始 ADC 值 */
for (int i = 0; i < 8; i++) {
    printf("ADC[%d]: %d ", i, grayscale_data.sensor[i]);
}
printf("\r\n");
```

### 3. 步进电机不转

```c
/* 逐步测试每个线圈 */
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);   // 一秒
HAL_Delay(1000);
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // 一秒
HAL_Delay(1000);
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
/* ... 继续 PIN 10, 11 */
```

## 性能优化建议

| 模块 | 建议 | 效果 |
|------|------|------|
| MPU6050 | 增加采样时间或使用低通滤波 | 降低噪声 |
| 灰度传感器 | 增加 ADC 采样周期 | 提高精度 |
| DC 电机 | 增加软启动延迟 | 平稳启动 |
| 步进电机 | 减小步进延迟 | 更快响应 |

## 常见问题排查

**Q: 多个模块同时运行时其中一个不工作？**
A: 检查中断优先级，避免高优先级中断阻塞低优先级任务。

**Q: I2C 通信间歇性失败？**
A: 添加软件 I2C 重试机制或增加电阻上拉值（通常 4.7kΩ）。

**Q: ADC 值飘动？**
A: 增加采样时间、添加硬件滤波电容、或使用移动平均滤波算法。

**Q: 步进电机震动但不转？**
A: 检查延迟是否过小（推荐 > 2ms），或电流是否不足。

## 参考文档

- MPU6050: [sensor-modules-guide.md](rules/sensor-modules-guide.md) 第 1 节
- 灰度传感器: [sensor-modules-guide.md](rules/sensor-modules-guide.md) 第 2 节
- 步进电机: [sensor-modules-guide.md](rules/sensor-modules-guide.md) 第 3 节
- 硬件集成: [hardware-integration.md](rules/hardware-integration.md)
- 最佳实践: [embedded-best-practices.md](rules/embedded-best-practices.md)
- 实际硬件清单: [memory/real-hardware-modules.md](memory/real-hardware-modules.md)
- I2C 快速参考: [memory/quick-i2c-ref.md](memory/quick-i2c-ref.md)
