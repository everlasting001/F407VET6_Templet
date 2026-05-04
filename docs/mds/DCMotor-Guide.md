# DCMotor — DC 有刷电机使用指南

> **适用硬件**: JGA25-310 有刷直流电机 + TB6612FNG 驱动板
> **基类**: [MotorBase](../../Devices/Inc/DeviceClass/Motors/MotorBase.h)
> **对照参考**: [StepperMotor 使用指南](StepperMotor-Guide.md)
> **引脚映射**: [Config.md](../../Config.md)

---

## 1. 架构概览

DCMotor 是 MotorBase 的子类，遵循 C-based OOP 虚函数表架构：

```
MotorBase (基类)
  ├─ vtable → init / run / cleanup / reset
  └─ 公有接口: Motor_Init / Motor_Run / Motor_Cleanup

DCMotor_t (子类)
  ├─ MotorBase base;        ← 必须为第一个成员
  ├─ DCMotorPinConfig *pins; ← AIN1/AIN2 引脚数组
  ├─ TIM_HandleTypeDef *htim; ← PWM 定时器句柄
  ├─ uint32_t tim_channel;   ← PWM 通道 (CH1/CH2)
  ├─ int8_t polarity;        ← 方向极性 (±1)
  ├─ uint16_t compare_value; ← 当前比较值 (0~2099)
  └─ DCMotorDir direction;  ← 当前方向
```

**多态分发路径**：`Motor_Init(&motor.base)` → `vtable->init()` → `DCMotor_init()`

---

## 2. 结构体字段

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `base` | `MotorBase` | — | 基类，必须为第一个成员 |
| `pins` | `const DCMotorPinConfig *` | NULL | 指向 2 元素引脚数组 |
| `htim` | `TIM_HandleTypeDef *` | NULL | PWM 定时器 (本项目为 `&htim3`) |
| `tim_channel` | `uint32_t` | 0 | PWM 通道 (`TIM_CHANNEL_1`/`TIM_CHANNEL_2`) |
| `polarity` | `int8_t` | `1` | 方向极性：`1`=CCW 正转，`-1`=CW 正转 |
| `compare_value` | `uint16_t` | `0` | 当前 TIM3 CCR 比较值 (0~2099) |
| `direction` | `DCMotorDir` | `FORWARD` | 当前方向：`DCMOTOR_DIR_FORWARD` 或 `DCMOTOR_DIR_BACKWARD` |

### 引脚配置结构体

```c
typedef struct {
    GPIO_TypeDef *port;    // GPIO 端口，如 GPIOD
    uint16_t      pin;     // GPIO 引脚，如 GPIO_PIN_0
} DCMotorPinConfig;

// 固定 2 个引脚，使用枚举索引：
typedef enum {
    DCMOTOR_PIN_AIN1 = 0,  // 方向控制引脚 1
    DCMOTOR_PIN_AIN2 = 1,  // 方向控制引脚 2
    DCMOTOR_PIN_COUNT = 2
} DCMotorPinIndex;
```

### 方向枚举

```c
typedef enum {
    DCMOTOR_DIR_FORWARD  = 0,  // 正转 (CCW, 逆时针)
    DCMOTOR_DIR_BACKWARD = 1,  // 反转 (CW, 顺时针)
} DCMotorDir;
```

---

## 3. TB6612FNG 方向控制

### 真值表 (polarity = 1, 默认)

| AIN1 | AIN2 | PWMA | OUT1 | OUT2 | 电机状态 |
|------|------|------|------|------|----------|
| 0 | 0 | X | 高阻 | 高阻 | **停止 (滑行)** |
| 1 | 0 | PWM | L | H | **正转 (CCW)** |
| 0 | 1 | PWM | H | L | **反转 (CW)** |
| 1 | 1 | X | L | L | **刹车** |

> **注**: OUT1/OUT2 的 H/L 与电机实际转向的关系取决于接线。若实际转向相反，翻转 `polarity` 即可（见第 6 节）。

### 极性字段

`polarity` 字段将 AIN1/AIN2 电平与"正转"解耦：

| polarity | 正转= | AIN1 | AIN2 | 适用场景 |
|----------|-------|------|------|----------|
| `1` (默认) | CCW 逆时针 | 1 | 0 | 接线与参考一致 |
| `-1` | CW 顺时针 | 0 | 1 | 接线与参考相反 |

---

## 4. TIM3 PWM 参数

### 时钟树计算

```
HSI (16MHz)
  → PLLM (/8)  =   2MHz
  → PLLN (×168) = 336MHz (VCO)
  → PLLP (/2)   = 168MHz (SYSCLK = HCLK)
  → APB1 (/4)   =  42MHz (PCLK1)
  → TIM3 (×2)   =  84MHz (APB1 prescaler ≠ 1, 定时器时钟翻倍)
  → PSC=1 (/2)  =  42MHz (计数器时钟)
  → ARR=2099    =  20kHz (PWM 频率)
```

| 参数 | 值 | 说明 |
|------|-----|------|
| TIM3 定时器时钟 | 84 MHz | 2 × APB1 (42MHz) |
| Prescaler (PSC) | 1 (÷2) | `htim3.Init.Prescaler = 2-1` |
| 计数器时钟 | 42 MHz | 84MHz / (1+1) |
| Auto-Reload (ARR) | 2099 | `htim3.Init.Period = 2100-1` |
| PWM 频率 | 20 kHz | 42MHz / (2099+1) |
| 占空比分辨率 | 0~2099 | 约 0.05% 步进 |
| PWM 模式 | PWM mode 1 | `TIM_OCMODE_PWM1` |
| 输出极性 | Active High | `TIM_OCPOLARITY_HIGH` |

> **20kHz 优点**：超出人耳可听范围（~20Hz–16kHz），电机运行时无啸叫声。

---

## 5. API 参考

### 5.1 DCMotor_Constructor

```c
void DCMotor_Constructor(
    DCMotor_t *self,
    const char *name,
    const DCMotorPinConfig *pins,
    TIM_HandleTypeDef *htim,
    uint32_t tim_channel
);
```

构造 DC 电机对象，注册虚函数表，设置默认值。

| 参数 | 说明 |
|------|------|
| `self` | 指向 DCMotor_t 实例的指针 |
| `name` | 电机名称 (用于调试，如 `"Left_DCMotor"`) |
| `pins` | 指向 2 元素 `DCMotorPinConfig` 数组的指针 |
| `htim` | PWM 定时器句柄 (本项目为 `&htim3`) |
| `tim_channel` | PWM 通道 (左电机=`TIM_CHANNEL_1`，右电机=`TIM_CHANNEL_2`) |

> **调用后**必须调用 `Motor_Init(&motor.base)` 完成初始化。

---

### 5.2 Motor_Init (基类)

```c
int Motor_Init(MotorBase *self);
```

通过虚函数表调用 `DCMotor_init()`：校验引脚/TIM 非空、拉低方向 GPIO、启动 PWM 通道并设比较值为 0。

| 返回值 | 说明 |
|--------|------|
| `0` | 初始化成功 |
| `-1` | 失败 (self 为 NULL、vtable 为空、pins 为空或 htim 为空) |

> **注意**：GPIO 模式由 CubeMX 生成的 `MX_GPIO_Init()` 预先配置，本函数仅设置初始电平。PWM 通过 `HAL_TIM_PWM_Start()` 开启，初始占空比为 0%。

---

### 5.3 DCMotor_SetSpeed

```c
void DCMotor_SetSpeed(DCMotor_t *self, int16_t speed);
```

设置电机速度和方向。正值=正转，负值=反转，幅值为比较寄存器值 (0~2099)。

| 参数 | 说明 |
|------|------|
| `self` | 电机对象指针 |
| `speed` | 有符号速度值：`>0`=正转，`<0`=反转，`0`=停止。幅值范围 0~2099，超限自动钳位至 2099 |

**行为细节**：
- `speed == 0` → 等价于 `DCMotor_Stop()`
- `speed > 0` → 正转方向 (`DCMOTOR_DIR_FORWARD`)
- `speed < 0` → 取绝对值，反转方向 (`DCMOTOR_DIR_BACKWARD`)
- 超出 `DCMOTOR_ARR_MAX` (2099) 的值自动钳位
- 同时写入方向 GPIO 和 `__HAL_TIM_SET_COMPARE()`
- 将 `base.state` 设为 `MOTOR_STATE_RUNNING`

**示例**：
```c
DCMotor_SetSpeed(&motor, 1050);   // 正转 ~50% 占空比
DCMotor_SetSpeed(&motor, -500);   // 反转 ~24% 占空比
DCMotor_SetSpeed(&motor, 0);      // 停止
DCMotor_SetSpeed(&motor, 2099);   // 正转 ~100% 占空比
```

---

### 5.4 DCMotor_Stop

```c
void DCMotor_Stop(DCMotor_t *self);
```

停止电机：PWM 比较值清零 + AIN1/AIN2 均拉低 (TB6612 滑行停止)。将 `base.state` 设为 `MOTOR_STATE_STOPPED`。

> **与 SetSpeed(0) 等效**。AIN1=0, AIN2=0 → TB6612 进入待机 (Standby)，电机自由滑行到停止。

---

### 5.5 DCMotor_SetDirection

```c
void DCMotor_SetDirection(DCMotor_t *self, DCMotorDir dir);
```

仅改变方向 GPIO 电平，不改变 PWM 比较值。若当前比较值 > 0 则状态保持 RUNNING。

| 参数 | 说明 |
|------|------|
| `dir` | `DCMOTOR_DIR_FORWARD` (正转) 或 `DCMOTOR_DIR_BACKWARD` (反转) |

> **与 SetSpeed 的区别**：SetDirection 保持当前速度仅换向，SetSpeed 需要传入完整的有符号速度值。若方向未变，函数直接返回 (无操作)。

---

### 5.6 DCMotor_GetSpeed

```c
uint16_t DCMotor_GetSpeed(const DCMotor_t *self);
```

返回当前 PWM 比较值 (0~2099)。**不包含方向信息**——仅返回幅值。

---

### 5.7 DCMotor_GetDirection

```c
DCMotorDir DCMotor_GetDirection(const DCMotor_t *self);
```

返回当前方向枚举值。

---

### 5.8 Motor_Run (基类)

```c
int Motor_Run(MotorBase *self);
```

在主循环中周期性调用。DC 电机子类的 `run()` 实现为空操作——PWM 由硬件 TIM3 自主产生，无需软件干预。保留此接口供未来扩展（如速度斜坡、过流检测等）。

---

### 5.9 Motor_Cleanup (基类)

```c
int Motor_Cleanup(MotorBase *self);
```

停止 PWM、拉低方向引脚、清除初始化标志。用于安全释放硬件资源。

---

## 6. 极性使用指南

### 问题场景

上电后调用 `DCMotor_SetSpeed(&motor, 500)`，预期电机正转 (逆时针)，但实际电机顺时针转动。

### 原因

电机 M+/M- 接线与 TB6612 OUT1/OUT2 的对应关系可能因项目而异。默认 `polarity = 1` 假设 OUT1→M+, OUT2→M- 对应 CCW 正转。若接线相反，则转向相反。

### 解决方案

**不需要改接线**。在 `Motor_Init()` 之前翻转极性：

```c
DCMotor_Constructor(&motor, "Left_DCMotor", pins, &htim3, TIM_CHANNEL_1);
motor.polarity = -1;   // ← 翻转极性
Motor_Init(&motor.base);
```

也可以在运行中动态修改：

```c
motor.polarity = -1;
DCMotor_SetSpeed(&motor, 500);  // 方向立即翻转
```

### 验证方法

编写最小测试：让电机低速正转 (比较值 ~300，足够克服静摩擦即可)，用肉眼观察转向是否符合预期。如相反则翻转 `polarity`。

```c
// 安全验证代码 (低速, 避免飞车)
DCMotor_SetSpeed(&motor, 300);   // ~14% 占空比
HAL_Delay(2000);                 // 观察 2 秒
DCMotor_Stop(&motor);
```

---

## 7. 引脚配置示例

### 左电机 (Left_DCMotor)

```c
#include "DCMotor.h"
#include "main.h"   // AIN1_Pin, AIN1_GPIO_Port 等宏定义

static const DCMotorPinConfig left_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {AIN1_GPIO_Port, AIN1_Pin},   // PD0
    [DCMOTOR_PIN_AIN2] = {AIN2_GPIO_Port, AIN2_Pin},   // PD1
};

DCMotor_t g_left_motor;

void Left_Motor_Init(void)
{
    DCMotor_Constructor(&g_left_motor, "Left_DCMotor", left_pins,
                        &htim3, TIM_CHANNEL_1);         // PA6
    Motor_Init(&g_left_motor.base);
}
```

### 右电机 (Right_DCMotor)

```c
static const DCMotorPinConfig right_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {BIN1_GPIO_Port, BIN1_Pin},   // PD2
    [DCMOTOR_PIN_AIN2] = {BIN2_GPIO_Port, BIN2_Pin},   // PD3
};

DCMotor_t g_right_motor;

void Right_Motor_Init(void)
{
    DCMotor_Constructor(&g_right_motor, "Right_DCMotor", right_pins,
                        &htim3, TIM_CHANNEL_2);         // PA7
    Motor_Init(&g_right_motor.base);
}
```

### 引脚速查表

| 电机 | AIN1 | AIN2 | PWMA (TIM3) | 编码器 A | 编码器 B |
|------|------|------|-------------|----------|----------|
| Left  | PD0 | PD1 | PA6 (CH1) | PE9 (TIM1_CH1) | PE11 (TIM1_CH2) |
| Right | PD2 | PD3 | PA7 (CH2) | PC6 (TIM8_CH1) | PC7 (TIM8_CH2) |

---

## 8. 完整最小示例

```c
#include "DCMotor.h"
#include "main.h"

// ---------- 引脚配置 ----------
static const DCMotorPinConfig left_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {AIN1_GPIO_Port, AIN1_Pin},
    [DCMOTOR_PIN_AIN2] = {AIN2_GPIO_Port, AIN2_Pin},
};

static const DCMotorPinConfig right_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {BIN1_GPIO_Port, BIN1_Pin},
    [DCMOTOR_PIN_AIN2] = {BIN2_GPIO_Port, BIN2_Pin},
};

// ---------- 电机对象 ----------
static DCMotor_t g_left_motor;
static DCMotor_t g_right_motor;

// ---------- 演示状态 ----------
static uint32_t s_last_tick = 0;
static uint8_t  s_step      = 0;

// ==================== Part 1: Init ====================
void DCMotor_Demo_Init(void)
{
    // 左电机
    DCMotor_Constructor(&g_left_motor, "Left_DCMotor", left_pins,
                        &htim3, TIM_CHANNEL_1);
    Motor_Init(&g_left_motor.base);

    // 右电机
    DCMotor_Constructor(&g_right_motor, "Right_DCMotor", right_pins,
                        &htim3, TIM_CHANNEL_2);
    Motor_Init(&g_right_motor.base);

    s_last_tick = HAL_GetTick();
}

// ==================== Part 2: Loop ====================
void DCMotor_Demo_Loop(void)
{
    Motor_Run(&g_left_motor.base);
    Motor_Run(&g_right_motor.base);

    uint32_t now = HAL_GetTick();
    if (now - s_last_tick < 3000) return;   // 每 3 秒切换
    s_last_tick = now;
    s_step = (s_step + 1) % 5;

    switch (s_step) {
    case 0:
        // 两电机正转 ~50%
        DCMotor_SetSpeed(&g_left_motor,  1050);
        DCMotor_SetSpeed(&g_right_motor, 1050);
        break;
    case 1:
        // 两电机反转 ~50%
        DCMotor_SetSpeed(&g_left_motor,  -1050);
        DCMotor_SetSpeed(&g_right_motor, -1050);
        break;
    case 2:
        // 差速: 左正转, 右反转
        DCMotor_SetSpeed(&g_left_motor,   1050);
        DCMotor_SetSpeed(&g_right_motor, -1050);
        break;
    case 3:
        // 全速正转
        DCMotor_SetSpeed(&g_left_motor,   2099);
        DCMotor_SetSpeed(&g_right_motor,  2099);
        break;
    case 4:
        // 停止
        DCMotor_Stop(&g_left_motor);
        DCMotor_Stop(&g_right_motor);
        break;
    }
}
```

在主函数中集成：

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    // ... 其他外设初始化 ...
    MX_TIM3_Init();

    DCMotor_Demo_Init();     // ← USER CODE BEGIN 2

    while (1) {
        DCMotor_Demo_Loop(); // ← USER CODE BEGIN 3
    }
}
```

---

## 9. 架构对比：DCMotor vs StepperMotor

| 特性 | DCMotor | StepperMotor |
|------|---------|-------------|
| 基类 | MotorBase | MotorBase |
| 电机类型 | DC 有刷 (JGA25-310) | 步进 (28BYJ-48) |
| 驱动芯片 | TB6612FNG (H 桥) | ULN2003 (达林顿阵列) |
| 控制方式 | PWM 占空比 + 方向 GPIO | 4 相脉冲序列 |
| 速度控制 | 比较寄存器值 (0~2099) | 步间间隔 (500~1500us) |
| 位置控制 | 需外部编码器 | 开环，步数级精度 |
| 定时器 | TIM3 (PWM 硬件自主) | 无 (DWT 软件时序) |
| 时序驱动 | 硬件 PWM | DWT 微秒级非阻塞 |
| 加减速 | 无 (PWM 瞬时响应) | 梯形加减速 |
| run() 行为 | 空操作 (硬件自主) | DWT 步进状态机 |
| 引脚数 | 2 (AIN1/AIN2) + PWM | 4 (IN1~IN4) |

---

## 10. 硬件接线注意事项

| 检查项 | 说明 |
|--------|------|
| **电源隔离** | TB6612 电机电源 (VM) 使用独立 7.4V，逻辑电源 (VCC) 使用 3.3V/5V。务必共地 |
| **去耦电容** | VM 端就近焊接 100μF 电解 + 0.1μF 陶瓷电容 |
| **PWM 频率** | 20kHz 高于音频范围，首次上电建议用示波器确认 PA6/PA7 波形正常 |
| **首次上电** | 使用低占空比 (~300/2099)，确认转向正确后再增大 |
| **堵转保护** | 当前软件未实现过流检测。长时间堵转 (DCMOTOR_ARR_MAX) 可能导致 TB6612 过温或电机烧毁 |
| **编码器** | 编码器由 TIM1/TIM8 独立处理，与 DCMotor 驱动无关 |
| **车体方向** | [Config.md](../../Config.md) 注明：左轮前进=CCW，右轮前进=CW。车体前进时左右电机方向相反 |

---

## 11. 参考文件

| 文件 | 说明 |
|------|------|
| [DCMotor.h](../../Devices/Inc/DeviceClass/Motors/DCMotor.h) | 结构体与 API 声明 |
| [DCMotor.c](../../Devices/Src/DeviceClass/Motors/DCMotor.c) | 完整实现 |
| [MotorBase.h](../../Devices/Inc/DeviceClass/Motors/MotorBase.h) | 基类接口 |
| [Config.md](../../Config.md) | 引脚映射与硬件配置 |
| [DCMotorTest.c](../../Application/Src/TestProgram/DCMotorTest.c) | 测试程序 |
| [StepperMotor-Guide.md](StepperMotor-Guide.md) | 步进电机使用指南 |
