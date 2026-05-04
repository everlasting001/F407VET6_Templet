# Skill: TB6612 直流电机控制

## 用途
为 STM32F407 项目配置和控制 TB6612 直流电机驱动模块。

## 使用方式
在 Roo Code 中通过 `skill("tb6612-motor-control", "<动作>")` 调用。

## 硬件配置

### 引脚分配
| 功能 | STM32 引脚 |
|------|-----------|
| PWMA | TIMx_CHy (PWM 输出) |
| AIN1 | GPIO (方向) |
| AIN2 | GPIO (方向) |
| PWMB | TIMx_CHz (PWM 输出) |
| BIN1 | GPIO (方向) |
| BIN2 | GPIO (方向) |
| STBY | GPIO (使能) |

> 具体引脚号请参考 CubeMX 配置。

## 控制代码模板

### 初始化
```c
void TB6612_Init(void)
{
    // 配置 PWM 定时器
    // 配置方向 GPIO
    // 配置 STBY GPIO

    HAL_TIM_PWM_Start(&htimx, TIM_CHANNEL_y);
    HAL_TIM_PWM_Start(&htimx, TIM_CHANNEL_z);
    
    // 拉高 STBY 使能驱动
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_SET);
    
    // 初始速度 0
    __HAL_TIM_SET_COMPARE(&htimx, TIM_CHANNEL_y, 0);
    __HAL_TIM_SET_COMPARE(&htimx, TIM_CHANNEL_z, 0);
}
```

### 电机控制函数
```c
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD,
    MOTOR_REVERSE,
    MOTOR_BRAKE
} MotorState_t;

void MotorA_Control(MotorState_t state, uint16_t speed)
{
    switch (state) {
        case MOTOR_FORWARD:
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
            __HAL_TIM_SET_COMPARE(&htimx, TIM_CHANNEL_y, speed);
            break;
        case MOTOR_REVERSE:
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
            __HAL_TIM_SET_COMPARE(&htimx, TIM_CHANNEL_y, speed);
            break;
        case MOTOR_BRAKE:
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
            __HAL_TIM_SET_COMPARE(&htimx, TIM_CHANNEL_y, 0);
            break;
        case MOTOR_STOP:
        default:
            HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
            __HAL_TIM_SET_COMPARE(&htimx, TIM_CHANNEL_y, 0);
            break;
    }
}
```

## 注意事项
- PWM 频率建议 10-50kHz
- 速度值范围取决于定时器 ARR 值
- 电机供电需与逻辑电源分开
