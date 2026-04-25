# 电机控制开发指南

## 支持的电机类型

### DC 有刷电机
**特点**：结构简单、控制容易、成本低

**控制方式**：
- 正反转：改变极性（H桥）
- 速度：PWM 调制

**要求**：
- 反电动势管理（续流二极管）
- 电流采样（大电流应用）
- 热保护

### 步进电机
**特点**：位置控制精确、无反馈环

**控制方式**：
- 脉冲序列驱动（4 相或 2 相）
- 速度由脉冲频率决定
- 扭矩由脉冲宽度决定

**常见驱动 IC**：ULN2003（低功率）、TB6560/TB6600（高功率）

### 无刷 DC 电机 (BLDC)
**特点**：高效率、长寿命、需要反馈

**控制方式**：
- 需要位置反馈（霍尔传感器或编码器）
- 根据反馈进行换向
- 通常采用 FOC（磁场定向控制）

**复杂度**：高

## PWM 控制基础

### PWM 参数选择

**频率选择**：
- DC 有刷电机：1-20 kHz（越高越平稳但功耗高）
- 推荐 10 kHz 作为起点
- 避免产生可听噪音（< 20 Hz 可能产生 20 kHz 谐波干扰音频）

**分辨率选择**：
- 8 位（0-255）：简单，分辨率低
- 10 位（0-1023）：平衡
- 16 位（0-65535）：精细控制，但计算开销大
- 推荐 10 位作为 STM32F407 的起点

**占空比计算**：
```
占空比 (%) = (比较值 / 周期值) × 100%
示例：PWM_VALUE = 512，PERIOD = 1024 → 50% 占空比 → 半速
```

### 死区时间配置

**作用**：防止 H 桥上下臂同时导通（贯穿电流）

**计算**：
```
死区时间 = (死区计数值 + 1) × 时钟周期
示例：如果时钟 168 MHz，要求 2 μs 死区
死区值 = (2μs × 168MHz) - 1 = 336 - 1 = 335
```

**配置方法**（STM32CubeMX）：
1. 选择定时器（TIM1/TIM8 有高级功能）
2. 启用 PWM + 互补输出
3. 设置死区时间

## 电机驱动集成指南

### 硬件接口

#### 单通道 PWM 驱动
```
STM32F407 PWM ──→ 驱动芯片 ──→ H桥/电机

配置：
- 1 个 PWM（占空比）
- 2 个 GPIO（方向控制）
```

#### 双 PWM 驱动（推荐）
```
STM32F407 PWM1 ──→ H桥上臂
          PWM2 ──→ H桥下臂
          
配置：
- 互补 PWM 输出
- 自动死区管理
- 更高效率
```

#### 电流采样
```
电机 ──→ [采样电阻] ──→ [模拟调理] ──→ STM32F407 ADC
          (0.1-1Ω)        (放大器)
```

### 软件实现架构

```c
// app_motor.h
typedef struct {
    uint16_t speed;        // 0-100% or 0-PWM_MAX
    uint8_t direction;     // 0: 停止, 1: 正转, 2: 反转
    uint8_t enabled;       // 使能标志
} MotorState_t;

extern MotorState_t motor_state;

void motor_init(void);
void motor_set_speed(uint16_t speed);
void motor_set_direction(uint8_t dir);
void motor_stop(void);
void motor_update(void);  // 中断或主循环调用
```

```c
// app_motor.c
#include "app_motor.h"

#define MOTOR_PWM_MAX 1000  // 对应 100% 占空比
#define MOTOR_PWM_MIN 0

MotorState_t motor_state = {
    .speed = 0,
    .direction = 0,
    .enabled = 0
};

void motor_init(void) {
    // 初始化 GPIO（方向控制）
    GPIO_InitStruct.Pin = GPIO_PIN_X | GPIO_PIN_Y;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 启动 PWM
    HAL_TIM_PWM_Start(&htim_motor, TIM_CHANNEL_X);
    
    motor_state.enabled = 1;
}

void motor_set_speed(uint16_t speed) {
    if (speed > MOTOR_PWM_MAX) speed = MOTOR_PWM_MAX;
    motor_state.speed = speed;
    __HAL_TIM_SET_COMPARE(&htim_motor, TIM_CHANNEL_X, speed);
}

void motor_set_direction(uint8_t dir) {
    motor_state.direction = dir;
    switch (dir) {
        case 0:  // 停止
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_X, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_Y, GPIO_PIN_RESET);
            break;
        case 1:  // 正转
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_X, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_Y, GPIO_PIN_RESET);
            break;
        case 2:  // 反转
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_X, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_Y, GPIO_PIN_SET);
            break;
    }
}

void motor_stop(void) {
    motor_set_speed(0);
    motor_set_direction(0);
}

void motor_update(void) {
    if (!motor_state.enabled) {
        motor_stop();
    }
    // 可在此添加电流过载保护、温度监控等
}
```

## 常见驱动芯片参考

### L298N（低功率、易获得）
- 输入：2-5V 逻辑信号
- 输出：最大 2A（每通道）
- 工作电压：5-35V
- 特点：集成 H 桥，无需外部 MOSFET
- 缺点：功耗大，需要散热

### TB6612（中等功率）
- 输入：3.3V/5V 逻辑
- 输出：最大 1.2A
- 工作电压：4.5-13.5V
- 特点：集成保护、小封装
- 适合：机器人、小型电机

### DRV8833（STM32 常搭配）
- 输入：3.3V 逻辑
- 输出：最大 1.5A
- 工作电压：2.7-10.8V
- 特点：集成功能丰富、易于使用
- 推荐用于 STM32F407 项目

## 电机控制常见问题

### 问题 1：电机抖动或不转
**排查步骤**：
1. 检查电源电压：供电不足或不稳定
2. 检查 PWM 信号：示波器测量频率和占空比
3. 检查方向控制：GPIO 电平是否正确
4. 检查电流：是否超过驱动芯片额定值

**解决方案**：
- 增加电源容量
- 检查驱动芯片发热情况
- 降低 PWM 频率测试

### 问题 2：电机噪音大
**可能原因**：
- PWM 频率太低（产生 20 kHz 谐波）
- 死区时间不合适
- 电流纹波大

**解决方案**：
- 增加 PWM 频率到 10-20 kHz
- 调整死区时间（通常 0.5-2 μs）
- 增加滤波电容

### 问题 3：加速/减速不平稳
**可能原因**：
- PWM 分辨率太低
- 低速脱力（电压太低）
- 控制环路不稳定

**解决方案**：
- 增加 PWM 分辨率（16 位）
- 设置最小占空比门限（通常 15-20%）
- 实现软启动逐渐加速

## 软启动实现

```c
// 缓慢加速，避免突然冲击
void motor_soft_start(uint16_t target_speed, uint16_t ramp_time_ms) {
    uint16_t current_speed = 0;
    uint16_t steps = ramp_time_ms / 10;  // 每 10ms 增加一步
    uint16_t increment = target_speed / steps;
    
    while (current_speed < target_speed) {
        current_speed += increment;
        if (current_speed > target_speed) current_speed = target_speed;
        
        motor_set_speed(current_speed);
        HAL_Delay(10);
    }
}
```

## 过载保护

```c
#define MOTOR_OVERCURRENT_THRESHOLD 1500  // mA

volatile uint16_t motor_current_ma = 0;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        // 将 ADC 值转换为电流（需要根据电路参数标定）
        motor_current_ma = adc_to_current(HAL_ADC_GetValue(hadc));
        
        if (motor_current_ma > MOTOR_OVERCURRENT_THRESHOLD) {
            motor_stop();
            error_flag |= ERROR_MOTOR_OVERCURRENT;
        }
    }
}
```

## 测试清单

- [ ] 电源电压在规定范围内
- [ ] PWM 信号正常（频率、占空比、波形）
- [ ] 方向控制逻辑正确
- [ ] 电机无突然启动或停止
- [ ] 电流不超过额定值
- [ ] 无异常噪音或烧焦味
- [ ] 温度在安全范围内
- [ ] 速度响应合理
