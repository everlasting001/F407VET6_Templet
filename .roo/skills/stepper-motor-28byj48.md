# Skill: 28BYJ-48 步进电机控制 (ULN2003)

## 用途
为 STM32F407 项目配置和控制 28BYJ-48 步进电机（通过 ULN2003 驱动）。

## 使用方式
在 Roo Code 中通过 `skill("stepper-motor-28byj48", "<动作>")` 调用。

## 8 步控制时序表

| 步进 | IN1 | IN2 | IN3 | IN4 |
|------|-----|-----|-----|-----|
| 1 | 1 | 0 | 0 | 0 |
| 2 | 1 | 1 | 0 | 0 |
| 3 | 0 | 1 | 0 | 0 |
| 4 | 0 | 1 | 1 | 0 |
| 5 | 0 | 0 | 1 | 0 |
| 6 | 0 | 0 | 1 | 1 |
| 7 | 0 | 0 | 0 | 1 |
| 8 | 1 | 0 | 0 | 1 |

## 代码模板

### 定义步进表
```c
const uint8_t step_sequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};
```

### 步进控制函数
```c
void Stepper_SetStep(uint8_t step)
{
    HAL_GPIO_WritePin(IN1_GPIO_Port, IN1_Pin,
        step_sequence[step][0] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IN2_GPIO_Port, IN2_Pin,
        step_sequence[step][1] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IN3_GPIO_Port, IN3_Pin,
        step_sequence[step][2] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(IN4_GPIO_Port, IN4_Pin,
        step_sequence[step][3] ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Stepper_Run(int16_t steps, uint16_t delay_ms)
{
    static uint8_t current_step = 0;
    uint8_t direction = (steps > 0) ? 1 : 0;
    uint16_t count = (steps > 0) ? steps : -steps;

    for (uint16_t i = 0; i < count; i++) {
        Stepper_SetStep(current_step);
        HAL_Delay(delay_ms);
        current_step = direction ? (current_step + 1) % 8
                                : (current_step + 7) % 8;
    }
}
```

## 参数说明
- 全步模式: 2048 步/圈（使用 4 步时序）
- 半步模式: 4096 步/圈（使用 8 步时序）
- 延时范围: 2-20ms（典型值）
- 延时越小速度越快，但扭矩会下降

## 建议
- 使用梯形加速/减速曲线避免失步
- 初始延时建议 10ms，逐步减小
