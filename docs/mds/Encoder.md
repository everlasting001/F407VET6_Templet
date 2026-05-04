# Encoder 编码器模块使用指南

## 概述

`Encoder` 是传感器设备类（Sensors）下的编码器子类，继承自 `SensorBase` 基类。采用 C 语言虚函数表（VTable）模式实现面向对象多态，提供霍尔编码器的脉冲计数、转速计算、线速度和距离解算功能。

## 硬件配置

### 编码器参数

| 参数 | 宏定义 | 值 | 说明 |
|------|--------|-----|------|
| 编码器线数 | `ENCODER_LINE` | 13 | 霍尔编码器磁环线数 |
| 减速比 | `GEAR_RATIO` | 20.409 | 电机减速比 |
| 单圈脉冲数 | `PULSE_PER_ROUND` | ~1061.27 | `ENCODER_LINE × 4 × GEAR_RATIO` |
| 车轮直径 | `WHEEL_DIAMETER` | 65.0 mm | 车轮直径 |
| 车轮周长 | `WHEEL_CIRCUMFERENCE` | ~204.20 mm | `WHEEL_DIAMETER × π` |
| 轮基距离 | `WHEEL_BASE_DISTANCE` | 125.0 mm | 左右轮间距 |

### 引脚接线

| 电机 | TIM 定时器 | 通道 A | 通道 B | 说明 |
|------|-----------|--------|--------|------|
| 左电机 | TIM1 | PE9 (CH1) | PE11 (CH2) | 编码器模式 TI1+TI2 |
| 右电机 | TIM8 | PC6 (CH1) | PC7 (CH2) | 编码器模式 TI1+TI2 |

### CubeMX 配置

1. **TIM1 / TIM8**：设置 Combined Channels 为 **Encoder Mode**，Polarity 为 Rising Edge
2. **TIM2**：配置为全局定时器基准（如 1ms 周期中断），40ms 更新周期由 ISR 中软件计数分频实现，NVIC 启用 TIM2 全局中断
3. **GPIO**：PE9/PE11、PC6/PC7 配置为 AF 模式（TIM1/TIM8）

## 架构设计

### 继承关系

```
SensorBase_t (基类)
    ├── name, initialized, update_period_ms
    └── vtable → {init, run, cleanup, reset}
          |
    Encoder_t (子类)
    ├── base: SensorBase_t (必须为第一个成员)
    ├── tim_handle: TIM_HandleTypeDef*
    ├── current_cnt, last_cnt, pulse_diff, total_pulse
    └── rpm, mmps, distance_mm
```

### 虚函数说明

| 虚函数 | 实现 | 功能 |
|--------|------|------|
| `init` | `Encoder_init` | 启动 TIM 编码器模式，清零数据 |
| `run` | `Encoder_run` | 读取脉冲 → 计算 RPM → 计算 MMPS → 累积距离 |
| `cleanup` | `Encoder_cleanup` | 停止 TIM 编码器，清零数据 |
| `reset` | `Encoder_reset` | 硬件计数器清零 + 软件数据清零 |

## 计算公式

### 更新周期

默认更新周期 40ms（25Hz），由 `Encoder_Constructor()` 自动设置，可通过 `base.update_period_ms` 修改。该 40ms 由 TIM2 全局定时器 ISR 中软件分频实现。

### 公式推导

```
脉冲差 = current_cnt - last_cnt  (int16_t 有符号运算，自动处理 16 位回绕)

RPM   = pulse_diff × 60 / update_period_s / PULSE_PER_ROUND

MMPS  = RPM × WHEEL_CIRCUMFERENCE / 60

距离增量 = MMPS × update_period_s
累积距离 += 距离增量
```

### 16 位计数器回绕处理

TIM 编码器计数器为 16 位（0~65535）。每次读取通过 `(int16_t)__HAL_TIM_GetCounter()` 强制转换为有符号数后与上次值做差，利用 int16_t 运算自动处理回绕：

```
示例：
  当前 = 100,  上次 = 65500 → 差值 = 100 - (-36) = 136 (正向)
  当前 = 65500, 上次 = 100   → 差值 = -36 - 100 = -136 (反向)
```

**约束**：更新周期必须足够短，确保单周期内脉冲变化不超过 ±32767。以 40ms 周期计算，最大可检测转速约 ±1852 RPM，远大于电机额定 460 RPM。

## API 参考

### 生命周期

```c
// 定义编码器对象
Encoder_t left_enc;

// 构造
void Encoder_Constructor(Encoder_t *self, TIM_HandleTypeDef *tim_handle, uint8_t motor_index, int8_t polarity);

// 初始化（基类接口，通过 vtable 调用 Encoder_init）
int SensorBase_Init(SensorBase_t *self);

// 周期性运行（基类接口，通过 vtable 调用 Encoder_run）
int SensorBase_Run(SensorBase_t *self);

// 清理（基类接口）
int SensorBase_Cleanup(SensorBase_t *self);

// 析构（基类接口）
void SensorBase_Destructor(SensorBase_t *self);
```

### 数据读取

```c
float    Encoder_GetRPM(const Encoder_t *self);        // 转速 (RPM)
float    Encoder_GetMMPS(const Encoder_t *self);       // 线速度 (mm/s)
float    Encoder_GetDistance(const Encoder_t *self);   // 累积距离 (mm)
int64_t  Encoder_GetTotalPulse(const Encoder_t *self); // 累计脉冲数
int32_t  Encoder_GetPulseDiff(const Encoder_t *self);  // 本次脉冲差
```

### 复位操作

```c
// 仅清零累积数据，保留硬件计数器运行（多段运动场景，如转弯后重新计数）
void Encoder_ClearData(Encoder_t *self);

// 完全复位（含硬件计数器清零 + 软件数据清零）
void Encoder_HardReset(Encoder_t *self);

// 基类复位接口（通过 vtable 调用 Encoder_reset，等同 HardReset）
void SensorBase_Reset(SensorBase_t *self);
```

### 调试打印

```c
// 打印单个编码器运动信息（内置 0.5s 速率限制）
// is_last=0: 行尾加 \r\n, is_last=1: 不加
void Encoder_PrintInfo(Encoder_t *self, DebugPrintf_t *dbg, char label, uint8_t is_last);

// 打印双编码器运动信息（内置 0.5s 速率限制）
// 输出 3 行: header\r\n + left data\r\n + right data（最后一行无 \r\n）
void Encoder_PrintDualInfo(Encoder_t *left, Encoder_t *right, DebugPrintf_t *dbg);
```

**输出格式示例**（`Encoder_PrintDualInfo`）：

```
=== Encoder t=12.500s ===
[L] RPM=123.4 Speed=456.7mm/s Dist=100.0mm Pulse=500 Diff=10
[R] RPM=122.0 Speed=450.0mm/s Dist=98.0mm Pulse=490 Diff=9
```

**速率限制说明**：
- 每个 Encoder 实例独立跟踪 `last_print_tick`，0.5s 内重复调用静默跳过
- `Encoder_PrintDualInfo` 以左编码器的 `last_print_tick` 为基准
- `Encoder_ClearData` 和 `Encoder_HardReset` 会重置 `last_print_tick`，复位后立即允许打印
- `dbg` 参数为 NULL 时静默返回（安全无操作）

## 使用示例

### 基本用法

```c
#include "Encoder.h"

Encoder_t left_enc;
Encoder_t right_enc;

void setup(void) {
    // 构造
    Encoder_Constructor(&left_enc,  &htim1, 0, -1);  // 左电机, TIM1, 极性反转
    Encoder_Constructor(&right_enc, &htim8, 1, +1);  // 右电机, TIM8

    // 初始化
    SensorBase_Init((SensorBase_t *)&left_enc);
    SensorBase_Init((SensorBase_t *)&right_enc);
}

// 在 TIM2 全局定时器 ISR 中软件分频后调用（40ms 间隔）
void TIM2_IRQ_Handler(void) {
    SensorBase_Run((SensorBase_t *)&left_enc);
    SensorBase_Run((SensorBase_t *)&right_enc);
}
```

### 串口打印运动信息

```c
// 在主循环中周期性打印双编码器数据（主循环中调用，非 ISR）
// Encoder_PrintDualInfo 内置 0.5s 节流，可安全高频调用
void main_loop(void) {
    Encoder_PrintDualInfo(&left_enc, &right_enc, &dbg_printf);
    // 其他循环任务...
}
```

### 多段运动场景（转弯后重新计数）

```c
// 转弯前：累积了一段距离
float dist_before_turn = Encoder_GetDistance(&left_enc);

// 转弯后：仅清零数据，不清零硬件计数器
Encoder_ClearData(&left_enc);
Encoder_ClearData(&right_enc);

// 继续下一段的距离累积...
float dist_after_turn = Encoder_GetDistance(&left_enc);
```

### 完全复位

```c
// 系统停止或紧急复位，硬件计数器也归零
Encoder_HardReset(&left_enc);

// 或通过基类接口
SensorBase_Reset((SensorBase_t *)&left_enc);
```

## 集成到 main.c

在 `main.c` 的 USER CODE 区域添加：

```c
/* USER CODE BEGIN 2 */
Encoder_Test_Init();
/* USER CODE END 2 */

while (1) {
    /* USER CODE BEGIN 3 */
    Encoder_Test_Loop();
    /* USER CODE END 3 */
}
```

在 `Callback.c` 中添加中断回调：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        Encoder_Test_IRQHandler();
    }
}
```

## 文件清单

| 文件 | 路径 | 说明 |
|------|------|------|
| SensorBase.h | `Devices/Inc/DeviceClass/Sensors/` | 传感器基类头文件 |
| SensorBase.c | `Devices/Src/DeviceClass/Sensors/` | 传感器基类实现 |
| Encoder.h | `Devices/Inc/DeviceClass/Sensors/` | 编码器子类头文件（含配置宏）|
| Encoder.c | `Devices/Src/DeviceClass/Sensors/` | 编码器子类实现 |
| EncoderTest.h | `Application/Inc/TestProgram/` | 编码器测试头文件（三分法接口）|
| EncoderTest.c | `Application/Src/TestProgram/` | 编码器测试实现 |

## 注意事项

1. **更新周期**：Run() 必须按固定周期调用，否则 RPM 和距离计算不准确
2. **ISR 中调用 Run**：推荐在定时中断中调用 Run()，确保精确时序
3. **ClearData vs HardReset**：多段计数用 ClearData（保留硬件计数器），完全复位用 HardReset
4. **浮点精度**：F407 有硬件 FPU，浮点计算在 ISR 中安全（需确保 FPU 上下文保存已配置）
5. **int16_t 回绕**：依赖更新周期足够短，确保不溢出 ±32767 范围
