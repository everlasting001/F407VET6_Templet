# StepperMotor 运行原理：DWT 非阻塞步进控制

## 架构概览

```
main() 主循环 (~kHz 频率)
  └─ Motor_Run(&motor.base)          ← 每循环周期调用
       └─ StepperMotor_run()          ← 虚函数, 实际执行步进逻辑
            ├─ DWT_ElapsedUs()        ← 检查是否到了该走下一步
            ├─ stepper_write_phases() ← 切换 4 相 GPIO
            └─ stepper_update_trapezoid() ← 更新下一步的间隔时间
```

与传统阻塞方式（`Delay_us(800)` 忙等 800us）不同，本实现**完全不阻塞**。主循环可以同时处理其他任务（LED 刷新、按键扫描、传感器读取等），步进电机的时序由 DWT 硬件计数器保证。

---

## 一、DWT 是什么？

DWT (Data Watchpoint and Trace) 是 ARM Cortex-M 内核内置的调试单元。它包含一个 **32 位周期计数器 (CYCCNT)**：

```
DWT->CYCCNT 每 1 个 CPU 时钟周期自动 +1
  ↓
168MHz → 每秒 +168,000,000 → 每微秒 +168

32 位：最大约 2³² / 168M ≈ 25.6 秒溢出，回绕到 0 重新开始
```

我们用这个计数器作为**高精度时钟源**，分辨率约 5.95ns（一个时钟周期）。

### 为什么无符号减法自动处理溢出？

```c
uint32_t now   = DWT->CYCCNT;   // 比如 0xFFFFFF00 (即将溢出)
uint32_t start = 0xFFFFF000;    // 起点

// 传统思维：now < start？出错了？
// 实际上：无符号减法
uint32_t elapsed = now - start;
// = 0xFFFFFF00 - 0xFFFFF000 = 0x00000F00 = 3840 个周期 ✓

// 即使溢出回绕到 0：
// DWT->CYCCNT = 0x00000100 (溢出后)
// elapsed = 0x00000100 - 0xFFFFF000 = 0x00001100 = 4352 个周期 ✓
//
// 无符号算术自动处理：回绕后差值依然正确
// 唯一限制：两次读数之间的时间不能超过 2³² 周期 ≈ 25.6 秒
```

这就是 DWT 的"溢出安全"特性——**无需任何条件判断或特殊处理**。

---

## 二、DWT 如何实现非阻塞？

### 对比：阻塞 vs 非阻塞

**阻塞方式（参考代码的做法）**：
```c
// 坏：CPU 卡在这里 800us，什么也做不了
void StepMotor_Run(...) {
    for (uint16_t i = 0; i < totalSteps; i++) {
        SetMotorPins(motor, ...);   // 切换相
        Delay_us(800);              // ← CPU 忙等 800us，阻塞！
    }
}
```

**非阻塞方式（我们的做法）**：
```c
// 好：每次调用 ≤ 几微秒，立即返回
static int StepperMotor_run(void *self) {
    StepperMotor_t *motor = (StepperMotor_t *)self;

    // Step 1: 检查该走了吗？
    uint32_t elapsed = DWT_ElapsedUs(motor->last_step_tick);
    if (elapsed < motor->step_interval_us) {
        return 0;   // 还没到时间，立即返回（< 1us）
    }

    // Step 2: 时间到了，走一步
    stepper_write_phases(motor, motor->step_index);  // 切换 4 相 GPIO
    motor->last_step_tick = DWT_GetTick_us();        // 记录新起点

    // Step 3: 更新步序、脉冲数、角度、梯形状态机
    // ...
    return 0;  // 总耗时 < 5us
}
```

### 关键：`DWT_ElapsedUs()` 的实现

```c
uint32_t DWT_ElapsedUs(uint32_t start_us) {
    uint32_t now_us = DWT_GetTick_us();   // 读取 DWT->CYCCNT 并转换为 us
    return now_us - start_us;             // 无符号减法，溢出安全
}
```

`DWT_GetTick_us()` 也不阻塞：
```c
uint32_t DWT_GetTick_us(void) {
    return DWT->CYCCNT / s_cycles_per_us;  // 168,000,000 / 1,000,000 = 168
    // 一次 32 位除法，~12 CPU 周期，合计 < 0.1us
}
```

### 时序图

```
时间轴 (us):  0     800   1600   2400   3200
              |      |      |      |      |
主循环调用 run():
  t=0:    elapsed=0   < 800 → 返回
  t=10:   elapsed=10  < 800 → 返回
  t=20:   elapsed=20  < 800 → 返回
  ...      (每次检查耗时 < 1us)
  t=800:  elapsed=800 ≥ 800 → 走一步！last_step_tick = 800
  t=810:  elapsed=10  < 800 → 返回
  ...
  t=1600: elapsed=800 ≥ 800 → 走一步！last_step_tick = 1600
```

主循环每次调用 `run()` 只需要不到 1 微秒来检查，其余时间 CPU 可以做任何事情。

---

## 三、StepperMotor 整体流程

### 3.1 初始化流程

```
StepperMotor_Constructor()
  ├─ Motor_Constructor()        ← 基类初始化 (vtable, name, state)
  ├─ self->pins = pins           ← 绑定 4 相引脚配置
  ├─ 默认: step_mode=HALF_8, speed_grade=5
  └─ self->base.vtable = &stepper_vtable  ← 替换为子类虚函数表

Motor_Init(&motor.base)
  └─ StepperMotor_init()
       ├─ 释放全部 4 相 (GPIO=RESET)
       ├─ 脉冲数/角度 归零
       └─ 梯形状态 = DONE

DWT_Init()
  ├─ CoreDebug->DEMCR |= ...    ← 使能 DWT 模块
  ├─ DWT->CYCCNT = 0            ← 清零计数器
  └─ DWT->CTRL |= ...           ← 启动计数器
```

### 3.2 设定目标角度

```
StepperMotor_SetAngle(&motor, 9000)  // 90.00°
  │
  ├─ angle_to_pulses(9000, HALF_8)
  │   = 9000 × 4096 / 36000 = 1024 步
  │
  ├─ stepper_calc_trapezoid(1024):
  │   accel  = 1024/3 = 341 步
  │   cruise = 1024 - 341 - 341 = 342 步
  │   decel  = 341 步
  │
  └─ stepper_start_trapezoid():
      ├─ cruise_interval = 800us  (grade 5)
      ├─ start_interval  = 800×2 = 1600us
      ├─ step_interval_us = 1600us  (从加速段最慢开始)
      ├─ last_step_tick = DWT_GetTick_us()
      └─ state = MOTOR_STATE_RUNNING
```

### 3.3 主循环 — 梯形加减速运行

```
每次 Motor_Run() → StepperMotor_run():
  │
  ├─ 检查: state == RUNNING?
  │   否 → 返回
  │
  ├─ 检查: 距离上一步过了多久?
  │   elapsed < step_interval_us → 返回 (还没到时间)
  │
  ├─ 写入拍序: 根据 step_index 设置 4 相 GPIO
  │   step_index=0: {A=1, B=0, C=0, D=0}
  │   step_index=1: {A=1, B=1, C=0, D=0}
  │   ...
  │
  ├─ 更新 last_step_tick = 当前 DWT 时间戳
  │
  ├─ 更新步序索引 (step_index ± 1)
  ├─ 更新脉冲计数 (current_pulses ± 1)
  ├─ 更新角度 = pulses_to_angle(current_pulses)
  │
  ├─ 梯形状态机:
  │   phase_step_count++
  │   if (steps_taken < accel_steps)      → ACCEL 阶段
  │   elif (steps_taken < a+c)            → CRUISE 阶段
  │   elif (steps_taken < total)          → DECEL 阶段
  │   else                                → DONE, state=STOPPED
  │
  └─ stepper_update_trapezoid():
      根据当前阶段和进度，线性计算下一步的间隔
      ACCEL:  1600us → 1600→800us  (越来越快)
      CRUISE: 800us  → 800us       (恒定)
      DECEL:  800us  → 800→1600us  (越来越慢)
```

---

## 四、梯形加速剖面可视化

```
1024 步行程 (90°), grade=5 (800us cruise):

步间间隔 (us)
  ↑
1600|●
    | ●●
1400|   ●●
    |     ●●●
1200|        ●●●
    |           ●●●●
1000|               ●●●●●
    |                    ●●●●●●
 800|                          ●●●●●●●●●●●●●●●●
    |                                            ●●●●●●●
 800|                                                     ●●●●●●
    |                                                            ●●●●●
 800|                                                                  ●●●●●
    |                                                                       ●●●●●●
 800|                                                                             ●●
    +──┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴──→ 步数
    0    加速段 (341步)      匀速段 (342步)       减速段 (341步)    1024
    
耗时估算:
  加速段: 平均 (1600+800)/2 = 1200us/步 × 341 = 0.41s
  匀速段: 800us/步 × 342 = 0.27s
  减速段: 同上 = 0.41s
  总计: ≈ 1.09s 完成 90° (比修复前的 8s 快了约 7 倍)
```

---

## 五、转速等级参考

| 等级 | 步间间隔 | 脉冲频率 | 4096 步/转耗时 | 参考 |
|------|---------|---------|---------------|------|
| 1 | 1500us | 667pps | ~6.1s | SLOW |
| 3 | 1000us | 1000pps | ~4.1s | |
| 5 | 800us | 1250pps | ~3.3s | **MEDIUM (默认)** |
| 7 | 600us | 1667pps | ~2.5s | |
| 9 | 500us | 2000pps | ~2.0s | FAST |
| 10 | 500us | 2000pps | ~2.0s | FAST (上限) |

数据来源：`docs/reference/User/03BSP/Src/steppermotor.c` 实践验证。

---

## 六、为什么比之前快了这么多？

| 问题 | 原因 | 修复 |
|------|------|------|
| 角度只有预期的一半 | `STEPS_PER_REV = 2048` 少了 50% | → 4096 |
| 默认速度慢 10 倍 | 8ms/步 (125pps) | → 800us/步 (1250pps) |
| TIM2 限制 | 1ms 分辨率，最快 1000pps | → DWT，微秒级，不限速 |
| 梯形起始太慢 | start = cruise × 3 | → × 2 |
| 中断开销 | 每次 ISR 检查判断 | → 无 ISR 依赖 |

---

## 七、调试提示

### 验证角度是否正确

```c
StepperMotor_SetAngle(&motor, 9000);   // 90.00°
// 预期: angle_to_pulses(9000, HALF_8) = 9000 × 4096 / 36000 = 1024 步
// 用调试器查看: motor.total_steps 应为 1024
```

### 验证时序精度

```c
// 在 StepperMotor_run() 中 step_flag 触发处加断点
// 用示波器测任意一个 IN 引脚的波形
// grade 5: 应见 800us 间隔的脉冲 ≈ 1.25kHz 方波
```

### 如果电机不转

1. 确认 `DWT_Init()` 在 `StepperMotor_Test_Init()` 中被调用
2. 确认 `Motor_Run()` 在主循环中被周期性调用
3. 用示波器检查 4 个 IN 引脚是否有电平变化
4. 确认 ULN2003 驱动板供电 (5V) 正常
