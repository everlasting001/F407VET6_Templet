# 传感器与模块集成指南

本指南覆盖 STM32F407VET6 项目中常用的传感器和驱动模块集成。

## 支持的硬件模块

### 1. MPU6050 - 6轴 IMU 传感器

**硬件特性**：
- 3 轴加速度计（AccelX/Y/Z）
- 3 轴陀螺仪（GyroX/Y/Z）
- 内置温度传感器
- 通信协议：I2C （地址 0x68 或 0x69）
- 采样率：可配置（最高 8 kHz）
- 工作电压：3.3V-5V（建议 3.3V）

**应用场景**：
- 机器人姿态控制
- 运动检测
- 零重力检测
- 角速度测量

**与 STM32F407 的接线**：
```
MPU6050    STM32F407
VCC    ──→ 3.3V
GND    ──→ GND
SCL    ──→ PB8 (I2C3_SCL) 或 PB6 (I2C1_SCL)
SDA    ──→ PB9 (I2C3_SDA) 或 PB7 (I2C1_SDA)
INT    ──→ PA15 (GPIO - 可选中断）
```

**I2C 寄存器地址**（速查表）：
```
0x3B-0x40  → 加速度计数据 (ACCEL_XOUT_H to ACCEL_ZOUT_L)
0x43-0x48  → 陀螺仪数据 (GYRO_XOUT_H to GYRO_ZOUT_L)
0x41-0x42  → 温度数据 (TEMP_OUT_H/L)
0x6B       → 电源管理 (PWR_MGMT_1) - 初始化时需要设置
0x1A       → 低通滤波配置 (CONFIG)
```

**初始化代码模板**：
```c
#include "stm32f4xx_hal.h"

// MPU6050 定义
#define MPU6050_ADDR 0x68  // I2C 地址 (8 位格式)
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B

typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    int16_t temp;
} MPU6050_Data_t;

extern I2C_HandleTypeDef hi2c1;  // 确保在 STM32CubeMX 中配置了 I2C1

// 初始化 MPU6050
int mpu6050_init(void) {
    uint8_t data = 0x00;
    // 唤醒 MPU6050（清除睡眠位）
    if (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, PWR_MGMT_1, 
                          I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK) {
        return -1;  // 失败
    }
    return 0;  // 成功
}

// 读取 MPU6050 数据
int mpu6050_read(MPU6050_Data_t *data) {
    uint8_t buffer[14];
    
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR << 1, ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buffer, 14, HAL_MAX_DELAY) != HAL_OK) {
        return -1;
    }
    
    // 转换为 16 位有符号整数（高字节优先）
    data->accel_x = (int16_t)(buffer[0] << 8 | buffer[1]);
    data->accel_y = (int16_t)(buffer[2] << 8 | buffer[3]);
    data->accel_z = (int16_t)(buffer[4] << 8 | buffer[5]);
    data->temp    = (int16_t)(buffer[6] << 8 | buffer[7]);
    data->gyro_x  = (int16_t)(buffer[8] << 8 | buffer[9]);
    data->gyro_y  = (int16_t)(buffer[10] << 8 | buffer[11]);
    data->gyro_z  = (int16_t)(buffer[12] << 8 | buffer[13]);
    
    return 0;
}
```

---

### 2. 八路灰度循迹模块 - 红外传感器阵列

**硬件特性**：
- 8 个红外光学传感器
- 模拟输出（需要 ADC 读取）或数字输出（GPIO）
- 检测黑白边界
- 工作电压：5V（部分 3.3V 兼容版本）
- 输出：8 个独立信号

**应用场景**：
- 自动循迹小车（黑线跟踪）
- 路径识别
- 障碍物检测

**接线方案（模拟输出版）**：
```
传感器模块    STM32F407
VCC       ──→ 5V
GND       ──→ GND
OUT1-OUT8 ──→ PA0-PA7 (ADC1_IN0-IN7) 或任意 GPIO
```

**接线方案（数字输出版）**：
```
传感器模块    STM32F407
VCC       ──→ 5V
GND       ──→ GND
D1-D8     ──→ PB0-PB7 (GPIO 输入)
```

**模拟版本读取代码**：
```c
#include "stm32f4xx_hal.h"

extern ADC_HandleTypeDef hadc1;

#define GRAYSCALE_CHANNELS 8
#define GRAYSCALE_ADC_SEQUENCE ADC_CHANNEL_0, ADC_CHANNEL_1, /* ... */ ADC_CHANNEL_7

typedef struct {
    uint16_t sensor[8];  // 8 个传感器的原始 ADC 值
    uint8_t binary[8];   // 二值化结果（0 or 1）
    int position;        // 黑线位置 (-35 到 +35)
} GrayscaleData_t;

// 初始化灰度传感器（在 STM32CubeMX 中配置 ADC1 8 通道）
void grayscale_init(void) {
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)grayscale_data.sensor, GRAYSCALE_CHANNELS);
}

// 读取所有 8 路传感器
void grayscale_read(GrayscaleData_t *data) {
    // ADC 值已通过 DMA 自动填充到 data->sensor[]
    
    // 二值化（假设阈值为 2048）
    for (int i = 0; i < 8; i++) {
        data->binary[i] = (data->sensor[i] > 2048) ? 1 : 0;
    }
    
    // 计算黑线位置（加权计算）
    // 示例：中心为 0，左偏负，右偏正
    int sum = 0, weight_sum = 0;
    for (int i = 0; i < 8; i++) {
        if (data->binary[i] == 1) {  // 检测到黑色
            int weight = (i - 3) * 5;  // -15 to +15
            sum += weight;
            weight_sum++;
        }
    }
    data->position = weight_sum > 0 ? sum / weight_sum : 0;
}

// 主循环中使用
int main(void) {
    GrayscaleData_t grayscale;
    grayscale_init();
    
    while (1) {
        grayscale_read(&grayscale);
        
        // 根据 grayscale.position 调整电机速度
        if (grayscale.position < -5) {
            // 黑线在左边，左转
            motor_set_direction(LEFT_TURN);
        } else if (grayscale.position > 5) {
            // 黑线在右边，右转
            motor_set_direction(RIGHT_TURN);
        } else {
            // 直行
            motor_set_direction(STRAIGHT);
        }
        
        HAL_Delay(10);
    }
}
```

**数字版本读取代码**：
```c
void grayscale_read_digital(GrayscaleData_t *data) {
    // 直接读取 GPIO 引脚
    data->binary[0] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
    data->binary[1] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
    // ... 类似读取 PIN 2-7
}
```

---

### 3. 步进电机驱动模块

**硬件特性**：
- 常见型号：ULN2003（5V 4 相）、TB6560、TB6600 等
- 控制方式：脉冲 + 方向 或 4 相顺序驱动
- 扭矩恒定（不受速度影响）
- 精确位置控制（步数级精度）

**与 STM32F407 的接线**（ULN2003 示例）：
```
ULN2003 驱动模块    STM32F407
VCC              ──→ 5V
GND              ──→ GND
IN1-IN4          ──→ PA8, PA9, PA10, PA11 (GPIO 输出)
或者
PULSE            ──→ TIM2_CH1 (PWM)
DIR              ──→ PA12 (GPIO)
```

**步进电机控制原理**：

**四相顺序驱动**（ULN2003 模式）：
```
步数  IN1  IN2  IN3  IN4
1     1    0    0    0
2     1    1    0    0
3     0    1    0    0
4     0    1    1    0
5     0    0    1    0
6     0    0    1    1
7     0    0    0    1
8     1    0    0    1
```

**代码实现**：
```c
#include "stm32f4xx_hal.h"

extern GPIO_TypeDef *GPIOA;

#define STEPPER_IN1_PIN GPIO_PIN_8
#define STEPPER_IN2_PIN GPIO_PIN_9
#define STEPPER_IN3_PIN GPIO_PIN_10
#define STEPPER_IN4_PIN GPIO_PIN_11

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pins[4];
    uint8_t current_step;
    int32_t position;  // 当前步数
} Stepper_t;

// 四相顺序表
static const uint8_t stepper_sequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

void stepper_init(Stepper_t *stepper) {
    stepper->port = GPIOA;
    stepper->pins[0] = STEPPER_IN1_PIN;
    stepper->pins[1] = STEPPER_IN2_PIN;
    stepper->pins[2] = STEPPER_IN3_PIN;
    stepper->pins[3] = STEPPER_IN4_PIN;
    stepper->current_step = 0;
    stepper->position = 0;
}

// 单步前进
void stepper_step_forward(Stepper_t *stepper) {
    stepper->current_step = (stepper->current_step + 1) % 8;
    
    // 设置 4 个线圈
    for (int i = 0; i < 4; i++) {
        if (stepper_sequence[stepper->current_step][i]) {
            HAL_GPIO_WritePin(stepper->port, stepper->pins[i], GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(stepper->port, stepper->pins[i], GPIO_PIN_RESET);
        }
    }
    
    stepper->position++;
}

// 单步后退
void stepper_step_backward(Stepper_t *stepper) {
    stepper->current_step = (stepper->current_step + 7) % 8;
    
    for (int i = 0; i < 4; i++) {
        if (stepper_sequence[stepper->current_step][i]) {
            HAL_GPIO_WritePin(stepper->port, stepper->pins[i], GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(stepper->port, stepper->pins[i], GPIO_PIN_RESET);
        }
    }
    
    stepper->position--;
}

// 旋转 N 步（正数=前进，负数=后退）
void stepper_rotate_steps(Stepper_t *stepper, int steps, uint16_t delay_ms) {
    if (steps > 0) {
        for (int i = 0; i < steps; i++) {
            stepper_step_forward(stepper);
            HAL_Delay(delay_ms);  // 控制速度
        }
    } else {
        for (int i = 0; i < -steps; i++) {
            stepper_step_backward(stepper);
            HAL_Delay(delay_ms);
        }
    }
}

// 停止（释放线圈）
void stepper_stop(Stepper_t *stepper) {
    for (int i = 0; i < 4; i++) {
        HAL_GPIO_WritePin(stepper->port, stepper->pins[i], GPIO_PIN_RESET);
    }
}

// 使用示例
int main(void) {
    Stepper_t stepper;
    stepper_init(&stepper);
    
    // 旋转 200 步（28BYJ-48 完整一圈），每步 5ms
    stepper_rotate_steps(&stepper, 200, 5);
    
    // 反向旋转 100 步
    stepper_rotate_steps(&stepper, -100, 5);
    
    stepper_stop(&stepper);
}
```

**脉冲+方向模式**（TB6560 模式）：
```c
// 更高效的驱动方式
void stepper_move_pulse_dir(Stepper_t *stepper, int steps, uint16_t frequency_hz) {
    // 配置 PWM 输出脉冲到 PULSE 引脚
    // 设置 DIR 引脚控制方向
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, steps > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    // 配置定时器 PWM，频率 = frequency_hz
    // 脉冲数 = abs(steps)
}
```

---

## 多模块集成方案

### 组件优先级（中断和 ADC）

```
优先级  组件              中断    ADC     说明
─────────────────────────────────────────────────
0       MPU6050          I2C    -       实时加速度/角速度
1       灰度传感器       DMA    ADC1    路径检测
2       步进电机         计时器  -       位置控制
3       DC 电机          PWM    可选    速度控制
```

### GPIO 分配建议

```
Port  Pin    用途              模块
────────────────────────────────────────
PA0-7       灰度传感器 ADC     8路灰度
PA8-11      步进电机驱动       ULN2003
PA12        步进电机方向       方向控制
PB6-9       I2C1 (MPU6050)     MPU6050
```

### 中断优先级配置

```c
// 在 main.c 中配置
// I2C (MPU6050): 抢占优先级 1，子优先级 0
HAL_NVIC_SetPriority(I2C1_EV_IRQn, 1, 0);

// DMA (灰度传感器): 抢占优先级 2，子优先级 0
HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);

// 定时器 (步进电机): 抢占优先级 3，子优先级 0
HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
```

---

## 调试技巧

### MPU6050 不响应？
```c
// 检查清单
1. I2C 时钟是否启用？ __HAL_RCC_I2C1_CLK_ENABLE()
2. GPIO 引脚是否配置为 I2C 模式？ GPIO_MODE_AF_OD
3. 上拉电阻是否存在？（通常 4.7kΩ）
4. 地址是否正确？（0x68 or 0x69）
5. 电源是否接好？（3.3V）

// 诊断代码
uint8_t buf;
if (HAL_I2C_Mem_Read(&hi2c1, 0x68 << 1, 0x75, 1, &buf, 1, 100) == HAL_OK) {
    if (buf == 0x68) {
        printf("MPU6050 found! ID=0x68\r\n");
    }
} else {
    printf("MPU6050 not found\r\n");
}
```

### 灰度传感器读数不稳定？
```
1. ADC 通道数是否与循环次数匹配？
2. DMA 缓冲是否足够大？(≥ 8 × uint16_t)
3. ADC 采样时间是否足够长？(> 15 cycles)
4. 传感器是否脏污？(清洁镜片)
5. 光线是否充足？
```

### 步进电机不转？
```
1. 线圈供电是否正确？(5V)
2. GPIO 引脚是否配置为输出？
3. 延迟是否足够大？(建议 > 2ms 每步)
4. 是否达到扭矩上限？(减少负载)
```

---

## 快速参考表

| 模块 | I2C 地址 | ADC 通道 | GPIO | 优先级 | 数据手册 |
|------|---------|---------|------|--------|---------|
| MPU6050 | 0x68/0x69 | - | PB6-9 | 1 | mpu6050.pdf |
| 灰度传感器 | - | PA0-7 | - | 2 | grayscale.pdf |
| 步进电机 | - | - | PA8-11 | 3 | stepper.pdf |
| DC 电机 | - | - | PA5-6 | 4 | motor.pdf |

