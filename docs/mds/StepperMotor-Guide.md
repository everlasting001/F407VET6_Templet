# StepperMotor — 步进电机使用指南

> **适用硬件**: 28BYJ-48 四相八拍步进电机 + ULN2003 驱动板
> **基类**: [MotorBase](../../Devices/Inc/DeviceClass/Motors/MotorBase.h)
> **对照参考**: [DCMotor 使用指南](DCMotor-Guide.md)
> **引脚映射**: [Config.md](../../Config.md)

---

## 1. 架构概览

StepperMotor 是 MotorBase 的子类，遵循 C-based OOP 虚函数表架构。**全部时序由 DWT 周期计数器驱动，不依赖任何硬件定时器中断**。

```
MotorBase (基类)
  ├─ vtable → init / run / cleanup / reset
  └─ 公有接口: Motor_Init / Motor_Run / Motor_Cleanup

StepperMotor_t (子类)
  ├─ MotorBase base;          ← 必须为第一个成员
  ├─ StepperPinConfig *pins;  ← 4 相引脚数组
  ├─ 步进模式 & 拍序索引
  ├─ 角度/脉冲双追踪
  ├─ 梯形加减速参数 (DWT 微秒级)
  └─ 方向 & 状态
```

**多态分发路径**：`Motor_Init(&motor.base)` → `vtable->init()` → `StepperMotor_init()`

---

## 2. 结构体字段

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `base` | `MotorBase` | — | 基类，必须为第一个成员 |
| `pins` | `const StepperPinConfig *` | NULL | 指向 4 元素引脚数组 |
| `step_mode` | `StepperStepMode` | `STEP_MODE_HALF_8` | 当前步进模式 |
| `step_index` | `uint8_t` | `0` | 当前在拍序表中的位置 (0~7) |
| `target_angle` | `int32_t` | `0` | 目标角度 (定点小数, 0.01° 单位) |
| `current_angle` | `int32_t` | `0` | 当前角度 (定点小数, 0.01° 单位) |
| `target_pulses` | `int32_t` | `0` | 目标总脉冲数 |
| `current_pulses` | `int32_t` | `0` | 当前脉冲计数 |
| `speed_grade` | `uint8_t` | `5` | 转速等级 (1~10) |
| `accel_steps` | `uint32_t` | `0` | 加速段步数 |
| `cruise_steps` | `uint32_t` | `0` | 匀速段步数 |
| `decel_steps` | `uint32_t` | `0` | 减速段步数 |
| `total_steps` | `uint32_t` | `0` | 本次运动总步数 |
| `phase_step_count` | `uint32_t` | `0` | 当前段已走步数 |
| `trap_phase` | `TrapezoidPhase` | `TRAPEZOID_DONE` | 当前梯形段 |
| `step_interval_us` | `uint32_t` | `800` | 当前步间间隔 (us) |
| `last_step_tick` | `uint32_t` | `0` | 上一步的 DWT 时间戳 |
| `direction` | `uint8_t` | `0` | 方向: `0`=正转(角度增加), `1`=反转 |

### 引脚配置结构体

```c
typedef struct {
    GPIO_TypeDef *port;    // GPIO 端口，如 GPIOD
    uint16_t      pin;     // GPIO 引脚，如 GPIO_PIN_4
} StepperPinConfig;
// 固定 4 个引脚对应 IN1/IN2/IN3/IN4
```

### 步进模式枚举

```c
typedef enum {
    STEP_MODE_HALF_8 = 0,  // 半步八拍 (默认, 力矩平滑, 4096 步/转)
    STEP_MODE_FULL_4 = 1,  // 整步四拍 (高力矩, 2048 步/转)
    STEP_MODE_WAVE_4 = 2,  // 波驱动四拍 (低力矩, 2048 步/转)
} StepperStepMode;
```

### 梯形加减速阶段枚举

```c
typedef enum {
    TRAPEZOID_ACCEL = 0,   // 加速段: 步间间隔从 start_interval 递减到 cruise_interval
    TRAPEZOID_CRUISE = 1,  // 匀速段: 步间间隔保持 cruise_interval
    TRAPEZOID_DECEL = 2,   // 减速段: 步间间隔从 cruise_interval 递增到 start_interval
    TRAPEZOID_DONE   = 3,  // 运动完成
} TrapezoidPhase;
```

---

## 3. 步进模式详解

### 拍序表

**半步八拍 (Half-Step 8)** — 默认推荐模式：

| Step | IN1 | IN2 | IN3 | IN4 | 通电相数 | 力矩 |
|------|-----|-----|-----|-----|----------|------|
| 0 | 1 | 0 | 0 | 0 | 1 | — |
| 1 | 1 | 1 | 0 | 0 | 2 | ↑ |
| 2 | 0 | 1 | 0 | 0 | 1 | — |
| 3 | 0 | 1 | 1 | 0 | 2 | ↑ |
| 4 | 0 | 0 | 1 | 0 | 1 | — |
| 5 | 0 | 0 | 1 | 1 | 2 | ↑ |
| 6 | 0 | 0 | 0 | 1 | 1 | — |
| 7 | 1 | 0 | 0 | 1 | 2 | ↑ |

**整步四拍 (Full-Step 4)**：

| Step | IN1 | IN2 | IN3 | IN4 |
|------|-----|-----|-----|-----|
| 0 | 1 | 1 | 0 | 0 |
| 1 | 0 | 1 | 1 | 0 |
| 2 | 0 | 0 | 1 | 1 |
| 3 | 1 | 0 | 0 | 1 |

**波驱动四拍 (Wave-Step 4)**：

| Step | IN1 | IN2 | IN3 | IN4 |
|------|-----|-----|-----|-----|
| 0 | 1 | 0 | 0 | 0 |
| 1 | 0 | 1 | 0 | 0 |
| 2 | 0 | 0 | 1 | 0 |
| 3 | 0 | 0 | 0 | 1 |

### 模式对比

| 模式 | 每周期步数 | 每转步数 | 步距角 | 力矩 | 功耗 | 推荐场景 |
|------|-----------|---------|--------|------|------|----------|
| `HALF_8` | 8 | **4096** | ~0.088° | 平滑 | 中 | 默认, 精度优先 |
| `FULL_4` | 4 | **2048** | ~0.176° | 较高 | 较高 | 需要大力矩 |
| `WAVE_4` | 4 | **2048** | ~0.176° | 最低 | 最低 | 低功耗保持 |

> **角度换算**：在 HALF_8 模式下，`target_angle = 9000` 表示 90.00°(9000/100)，对应 `9000/360 × 4096 = 102400` 步。详见第 6 节。

---

## 4. 转速等级

10 级转速由步间间隔 (μs) 定义，基于 28BYJ-48 实践验证：

| 等级 | 步间间隔 | 脉冲频率 | 4096 步/转耗时 | 说明 |
|------|---------|---------|---------------|------|
| 1 | 1500 μs | 667 pps | ~6.1 s/rev | 极慢 |
| 2 | 1200 μs | 833 pps | ~4.9 s/rev | 慢速 |
| 3 | 1000 μs | 1000 pps | ~4.1 s/rev | 中慢 |
| 4 | 900 μs | 1111 pps | ~3.7 s/rev | 中速偏慢 |
| **5** | **800 μs** | **1250 pps** | **~3.3 s/rev** | **中速 (默认)** |
| 6 | 700 μs | 1429 pps | ~2.9 s/rev | 中速偏快 |
| 7 | 600 μs | 1667 pps | ~2.5 s/rev | 快速 |
| 8 | 550 μs | 1818 pps | ~2.3 s/rev | 很快 |
| 9 | 500 μs | 2000 pps | ~2.0 s/rev | 极快 |
| 10 | 500 μs | 2000 pps | ~2.0 s/rev | 极快 (同 9 级) |

> **注意**：等级 9 和 10 的脉冲频率相同 (2000pps)，这是 28BYJ-48 的硬件极限。超出此频率可能导致失步。
>
> **实际最高转速约**：1 转/2 秒 = 30 rpm。远低于 DC 电机，但位置精度极高。

---

## 5. 梯形加减速

每次调用 `StepperMotor_SetAngle()` 触发一次完整运动，自动计算三段梯形速度曲线：

```
步间间隔 (us)
  ↑
  │ start ─╲
  │         ╲  加速段 (1/3)
  │          ╲________ cruise (匀速段, 1/3)
  │                   ╲
  │                    ╲  减速段 (1/3)
  │                     ╲________
  │
  └──────────────────────────────→ 步数
       ← 加速 →← 匀速 →← 减速 →
```

| 参数 | 说明 |
|------|------|
| 加速/减速占比 | 各占总步数的 **1/3** (最多各不超过 1/2) |
| 加速起始间隔 | 巡航间隔 × 2, 上限 3000μs |
| 极短行程 (< 3 步) | 跳过梯形，直接匀速 |
| 短行程 | 加速段 + 减速段直接衔接 (无巡航段) |

**修改加速段占比**：调整 [StepperMotor.c](../../Devices/Src/DeviceClass/Motors/StepperMotor.c) 中的 `ACCEL_RATIO_NUM` / `ACCEL_RATIO_DEN` 宏。

---

## 6. 角度与脉冲换算

### 换算公式

```
脉冲 → 角度: angle_centideg = pulses × 360 × 100 / steps_per_rev
角度 → 脉冲: pulses = angle_centideg × steps_per_rev / (360 × 100)
```

其中 `angle_centideg` 为定点小数, 单位 0.01°:
- `0` = 0.00°
- `9000` = 90.00°
- `-4500` = -45.00°
- `36000` = 360.00° (整圈)

`steps_per_rev` 取决于模式:

| 模式 | steps_per_rev |
|------|--------------|
| HALF_8 | 4096 |
| FULL_4 / WAVE_4 | 2048 |

### 示例

```c
// HALF_8 模式下旋转 90.00°
// 脉冲数 = 9000 × 4096 / (360 × 100) = 1024
StepperMotor_SetAngle(&motor, 9000);

// 复位到 0°
StepperMotor_ResetAngle(&motor);

// 反转 45.00°
StepperMotor_SetAngle(&motor, -4500);
```

---

## 7. DWT 非阻塞时序机制

### 为什么不用定时器中断？

传统的步进电机控制使用定时器 ISR 在每次中断中走一步。问题：
- ISR 频率可达 2000Hz (等级 10)，CPU 中断开销大
- 梯形加减速中需要动态调整中断周期，实现复杂
- 多个步进电机需要多个定时器

### DWT 方案

DWT (Data Watchpoint and Trace) 是 ARM Cortex-M4 内置的 32 位硬件周期计数器，以 CPU 时钟频率递增 (168MHz)。本方案使用它来测量微秒级时间间隔。

**核心思想**：在主循环 `StepperMotor_run()` 中反复检查：

```
当前时间 - 上一步时间 ≥ 步间间隔？
  ├─ 是 → 走一步 + 更新时间戳 + 梯形状态机推进
  └─ 否 → 立即返回 (不阻塞!)
```

### 调用时序图

```
main() while(1):
  Motor_Run(&motor.base)         ← 每次循环都调用
    → vtable->run()
      → StepperMotor_run()
        → DWT_ElapsedUs(last_step_tick) >= step_interval_us?
          ├─ No:  return 0;      ← 非阻塞, 立即返回
          └─ Yes: 写 GPIO 拍序 → 更新梯形参数 → return 0;
```

> **关键约束**：主循环速率必须足够快，确保 `Motor_Run()` 的调用间隔远小于步间间隔。在最高速度 (500μs/步) 下，主循环应小于 ~250μs 以避免失步。如果主循环中有阻塞操作，步进时序将受影响。

---

## 8. API 参考

### 8.1 StepperMotor_Constructor

```c
void StepperMotor_Constructor(
    StepperMotor_t *self,
    const char *name,
    const StepperPinConfig *pins
);
```

构造步进电机对象，注册虚函数表，设置默认值 (半步八拍、转速等级 5、角度归零)。

| 参数 | 说明 |
|------|------|
| `self` | 指向 StepperMotor_t 实例的指针 |
| `name` | 电机名称 (如 `"Vertical_Stepper"`) |
| `pins` | 指向 4 元素 `StepperPinConfig` 数组的指针 |

> **调用后**必须调用 `Motor_Init(&motor.base)` 完成初始化。

---

### 8.2 Motor_Init (基类)

```c
int Motor_Init(MotorBase *self);
```

通过虚函数表调用 `StepperMotor_init()`：校验引脚非空、拉低所有 4 相 GPIO、复位步进状态和角度计数器。

| 返回值 | 说明 |
|--------|------|
| `0` | 初始化成功 |
| `-1` | 失败 (self 为 NULL、vtable 为空或 pins 为 NULL) |

> **注意**：GPIO 模式由 CubeMX 生成的 `MX_GPIO_Init()` 预先配置，本函数仅设置初始电平 (全低 → 电机释放)。

---

### 8.3 StepperMotor_SetAngle

```c
void StepperMotor_SetAngle(StepperMotor_t *self, int32_t angle);
```

设置目标绝对角度，触发一次梯形加减速运动。**非阻塞**：函数立即返回，实际运动在后续的 `Motor_Run()` 调用中完成。

| 参数 | 说明 |
|------|------|
| `self` | 电机对象指针 |
| `angle` | 目标角度 (定点小数, 0.01° 单位, 如 `9000`=90.00°) |

**行为细节**：
- 计算目标脉冲数 (`angle_to_pulses`)
- 计算差值 (`delta = target_pulses - current_pulses`)
- `delta > 0` → 正转 (`direction = 0`)
- `delta < 0` → 反转 (`direction = 1`)
- `delta == 0` → 直接返回 (无运动)
- 调用 `stepper_calc_trapezoid()` 计算加/匀/减速步数
- 调用 `stepper_start_trapezoid()` 启动加速段
- 如果电机正在运动，先调用 `StepperMotor_EmergencyStop()` 再开始新运动

**示例**：
```c
StepperMotor_SetAngle(&motor, 9000);   // 旋转到 90.00°
StepperMotor_SetAngle(&motor, 0);      // 旋转回 0.00°
StepperMotor_SetAngle(&motor, -18000); // 反转 180.00°
```

---

### 8.4 StepperMotor_ResetAngle

```c
void StepperMotor_ResetAngle(StepperMotor_t *self);
```

紧急停止后将目标角度和当前角度均清零。用于重新标定零点。

> **典型场景**：触发限位开关后将当前位置设为原点。

---

### 8.5 StepperMotor_SetMode

```c
void StepperMotor_SetMode(StepperMotor_t *self, StepperStepMode mode);
```

切换步进模式。如果电机正在运动，先紧急停止。

| 参数 | 说明 |
|------|------|
| `mode` | `STEP_MODE_HALF_8` / `STEP_MODE_FULL_4` / `STEP_MODE_WAVE_4` |

> 切换模式后，当前角度不变，但 `current_pulses` 和 `target_pulses` 会按新模式重新换算。如果不同模式的每转步数不同，相同角度对应的脉冲数会变化。

---

### 8.6 StepperMotor_SetSpeed

```c
void StepperMotor_SetSpeed(StepperMotor_t *self, uint8_t grade);
```

设置转速等级 (1~10)。超限值自动钳位。如果电机正在运动，实时更新梯形速度参数。

| 参数 | 说明 |
|------|------|
| `grade` | 转速等级 1 (最慢) ~ 10 (最快)，默认 5 |

---

### 8.7 StepperMotor_GetAngle / StepperMotor_GetPulses

```c
int32_t StepperMotor_GetAngle(const StepperMotor_t *self);
int32_t StepperMotor_GetPulses(const StepperMotor_t *self);
```

返回当前角度 (0.01° 单位) 或当前脉冲计数。每步执行时自动更新。

---

### 8.8 StepperMotor_EmergencyStop

```c
void StepperMotor_EmergencyStop(StepperMotor_t *self);
```

立即停止：拉低所有 4 相线圈、状态设为 STOPPED、梯形状态归零。**不改变角度和脉冲计数**——停止后调用 `SetAngle()` 可继续运动。

---

### 8.9 Motor_Run (基类)

```c
int Motor_Run(MotorBase *self);
```

**这是步进电机的核心驱动函数，必须在主循环中高频调用。** 通过虚函数表调用 `StepperMotor_run()`：

1. 检查 `base.state == MOTOR_STATE_RUNNING`，否则直接返回
2. 通过 `DWT_ElapsedUs()` 检查是否到达步进时刻
3. 到达 → 写入当前拍序到 GPIO → 更新步序索引 → 更新脉冲/角度 → 梯形状态机推进 → 更新步间间隔
4. 未到达 → 立即返回 `0` (不阻塞)

> **调用频率要求**：主循环速率必须 > 步进频率。在最极端情况下 (等级 10, 2000pps)，主循环应 ≥ 4000Hz (每循环 < 250μs)。

---

### 8.10 Motor_Cleanup (基类)

```c
int Motor_Cleanup(MotorBase *self);
```

拉低所有 4 相线圈、清除初始化标志。用于安全释放硬件资源。

---

## 9. 引脚配置示例

### 水平步进电机 (Horizontal)

```c
#include "StepperMotor.h"
#include "main.h"   // H_IN1_Pin, H_IN1_GPIO_Port 等宏定义

static const StepperPinConfig h_pins[4] = {
    {H_IN1_GPIO_Port, H_IN1_Pin},   // IN1 → PD4
    {H_IN2_GPIO_Port, H_IN2_Pin},   // IN2 → PD5
    {H_IN3_GPIO_Port, H_IN3_Pin},   // IN3 → PD6
    {H_IN4_GPIO_Port, H_IN4_Pin},   // IN4 → PD7
};

StepperMotor_t g_horizontal_motor;

void Horizontal_Motor_Init(void)
{
    StepperMotor_Constructor(&g_horizontal_motor, "Horizontal", h_pins);
    Motor_Init(&g_horizontal_motor.base);
    StepperMotor_SetSpeed(&g_horizontal_motor, 5);
}
```

### 垂直步进电机 (Vertical)

```c
static const StepperPinConfig v_pins[4] = {
    {V_IN1_GPIO_Port, V_IN1_Pin},   // IN1 → PB3
    {V_IN2_GPIO_Port, V_IN2_Pin},   // IN2 → PB4
    {V_IN3_GPIO_Port, V_IN3_Pin},   // IN3 → PB5
    {V_IN4_GPIO_Port, V_IN4_Pin},   // IN4 → PB8
};

StepperMotor_t g_vertical_motor;

void Vertical_Motor_Init(void)
{
    StepperMotor_Constructor(&g_vertical_motor, "Vertical", v_pins);
    Motor_Init(&g_vertical_motor.base);
    StepperMotor_SetSpeed(&g_vertical_motor, 5);
}
```

### 引脚速查表

| 电机 | IN1 | IN2 | IN3 | IN4 |
|------|-----|-----|-----|-----|
| Horizontal | PD4 | PD5 | PD6 | PD7 |
| Vertical | PB3 | PB4 | PB5 | PB8 |

---

## 10. 完整最小示例

```c
#include "StepperMotor.h"
#include "dwt_delay.h"
#include "main.h"

// ---------- 引脚配置 ----------
static const StepperPinConfig pins[4] = {
    {V_IN1_GPIO_Port, V_IN1_Pin},
    {V_IN2_GPIO_Port, V_IN2_Pin},
    {V_IN3_GPIO_Port, V_IN3_Pin},
    {V_IN4_GPIO_Port, V_IN4_Pin},
};

// ---------- 电机对象 ----------
static StepperMotor_t g_motor;

// ---------- 演示状态 ----------
static uint32_t s_last_tick = 0;
static uint8_t  s_step      = 0;

// 测试角度序列 (0.01° 单位)
static const int32_t s_angles[] = {
    9000,   //  90.00°
    0,      //   0.00°
    -9000,  // -90.00°
    4500,   //  45.00°
};

// ==================== Part 1: Init ====================
void Stepper_Demo_Init(void)
{
    DWT_Init();  // 必须: 初始化 DWT 周期计数器

    StepperMotor_Constructor(&g_motor, "Demo_Stepper", pins);
    Motor_Init(&g_motor.base);

    StepperMotor_SetSpeed(&g_motor, 5);    // 中速
    StepperMotor_ResetAngle(&g_motor);     // 零点标定

    s_last_tick = HAL_GetTick();
}

// ==================== Part 2: Loop ====================
void Stepper_Demo_Loop(void)
{
    // 1. 必须高频调用!
    Motor_Run(&g_motor.base);

    // 2. 每 5 秒切换目标角度
    uint32_t now = HAL_GetTick();
    if (now - s_last_tick < 5000) return;
    s_last_tick = now;

    int32_t target = s_angles[s_step];
    s_step = (s_step + 1) % (sizeof(s_angles) / sizeof(s_angles[0]));

    StepperMotor_SetAngle(&g_motor, target);
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

    Stepper_Demo_Init();     // ← USER CODE BEGIN 2

    while (1) {
        Stepper_Demo_Loop(); // ← USER CODE BEGIN 3
    }
}
```

> **重要**：`DWT_Init()` 必须在 `StepperMotor_Constructor` 之前调用，否则 `DWT_GetTick_us()` 将返回无效值，导致步进时序异常。

---

## 11. 常见问题诊断

| 问题 | 可能原因 | 解决方案 |
|------|---------|----------|
| 电机不转 | GPIO 引脚未在 CubeMX 中配置为输出 | 检查 `.ioc` 文件 GPIO 设置 |
| 电机抖动但不转 | 速度太快 (等级过高) | 降低转速至 3~4 级 |
|  | 负载超过扭矩 | 减轻负载或切换到 FULL_4 模式 |
|  | 电源不足 | ULN2003 需 5V/≥500mA |
| 角度不准 | 失步 (速度过高 + 高负载) | 降低转速或使用 HALF_8 模式 |
|  | DWT_Init() 未调用 | 在构造函数前调用 DWT_Init() |
| 运行时突然停止 | 主循环被阻塞 | 检查是否有长时间 `HAL_Delay()` |
| 电机发热 | 长时间保持扭矩 | 不需要保持时调用 EmergencyStop 释放线圈 |
| 一拍走很慢 | DWT_ElapsedUs 未正常工作 | 检查 DWT_Init 是否调用、Core 时钟是否正确 |

---

## 12. 架构对比：StepperMotor vs DCMotor

| 特性 | StepperMotor | DCMotor |
|------|-------------|---------|
| 基类 | MotorBase | MotorBase |
| 电机类型 | 步进 (28BYJ-48) | DC 有刷 (JGA25-310) |
| 驱动芯片 | ULN2003 (达林顿阵列) | TB6612FNG (H 桥) |
| 控制方式 | 4 相脉冲序列 | PWM 占空比 + 方向 GPIO |
| 速度控制 | 步间间隔 (500~1500us) | 比较寄存器值 (0~2099) |
| 位置控制 | 开环，步数级精度 | 需外部编码器 |
| 定时器 | 无 (DWT 软件时序) | TIM3 (PWM 硬件自主) |
| 时序驱动 | DWT 微秒级非阻塞 | 硬件 PWM |
| 加减速 | 梯形加减速 | 无 (PWM 瞬时响应) |
| run() 行为 | DWT 步进状态机 | 空操作 (硬件自主) |
| 引脚数 | 4 (IN1~IN4) | 2 (AIN1/AIN2) + PWM |

---

## 13. 硬件接线注意事项

| 检查项 | 说明 |
|--------|------|
| **电源** | ULN2003 驱动板需独立 5V 供电 (≥500mA)，勿从 MCU 3.3V 取电 |
| **相序** | IN1→IN4 必须按顺序连接电机 A/B/C/D 相 (28BYJ-48 标准色序: 蓝/粉/黄/橙, 红=公共端接 5V) |
| **公共端** | 28BYJ-48 红色线必须接 5V (非 GND!) |
| **DWT 初始化** | 使用前必须调用 `DWT_Init()`，否则步进完全失效 |
| **主循环速率** | 不能有长时间阻塞。若必须使用 `HAL_Delay()`，每次延迟不超过 50ms |
| **释放线圈** | 停止后如需节能或减少发热，调用 `StepperMotor_EmergencyStop()` 释放所有线圈 |
| **28BYJ-48 实际步数** | 减速比约 1:64，半步模式 64×64=4096 步/转(输出轴)，整步模式 2048 步/转 |

---

## 14. 参考文件

| 文件 | 说明 |
|------|------|
| [StepperMotor.h](../../Devices/Inc/DeviceClass/Motors/StepperMotor.h) | 结构体与 API 声明 |
| [StepperMotor.c](../../Devices/Src/DeviceClass/Motors/StepperMotor.c) | 完整实现 |
| [MotorBase.h](../../Devices/Inc/DeviceClass/Motors/MotorBase.h) | 基类接口 |
| [Config.md](../../Config.md) | 引脚映射与硬件配置 |
| [dwt_delay.h](../../Devices/Inc/DebugPeripheral/dwt_delay.h) | DWT 微秒延时接口 |
| [StepperMotorTest.c](../../Application/Src/TestProgram/StepperMotorTest.c) | 测试程序 |
| [DCMotor-Guide.md](DCMotor-Guide.md) | DC 有刷电机使用指南 |
