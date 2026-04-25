# STM32F407VET6 分层架构实现总结

**完成日期**: 2026-04-25  
**编译状态**: ✅ 成功 (Debug 配置)  
**目标**: 为电赛项目建立规范化的分层架构和任务调度系统

---

## 📋 实现清单

### ✅ 已完成

#### 1. 框架层 (Core/Framework/)
- ✅ `system_tick.h/c` - 1ms 系统滴答定时器
- ✅ `task_scheduler.h/c` - 周期性任务调度器（支持 10+ 任务）
- ✅ `state_machine.h/c` - 通用有限状态机框架
- ✅ `device_base.h` - 设备驱动基类（虚函数指针模式）

**特性**：
- 无需 RTOS，轻量级设计
- 手动分频计数器实现任务周期
- 状态机支持 enter/process/exit 三段模式
- 虚函数指针表实现面向对象多态

#### 2. 设备驱动层 (Core/Drivers/)
- ✅ `motor_driver.h/c` - DC 有刷电机驱动示例
  - 支持 PWM 速度控制（0-1000）
  - 支持 GPIO 方向控制（正/反/停）
  - 虚函数指针表接口

**可扩展**：
- `sensor_imu.h/c` - IMU 传感器模板
- `sensor_grayscale.h/c` - 灰度传感器模板
- `stepper_motor.h/c` - 步进电机模板

#### 3. 应用层 (Core/App/)
- ✅ `app_config.h/c` - 应用初始化和任务示例
  - 演示 3 个不同周期的任务
  - 演示如何使用框架层 API

#### 4. 主程序集成
- ✅ `Core/Src/main.c` - 集成框架初始化
- ✅ `Core/Src/stm32f4xx_it.c` - SysTick 中断处理

**关键改动**：
```c
// main.c
int main(void) {
    system_tick_init(1000);    // 1ms 滴答
    app_init();                 // 初始化应用任务
    while (1) app_main_loop();
}

// stm32f4xx_it.c
void SysTick_Handler(void) {
    HAL_IncTick();
    system_tick_update();  // 驱动任务调度
}
```

#### 5. 构建配置
- ✅ `CMakeLists.txt` - 添加新文件和路径

**编译结果**：
```
Memory region         Used Size  Region Size  %age Used
             RAM:        1712 B       128 KB      1.31%
          CCMRAM:           0 B        64 KB      0.00%
           FLASH:        5784 B       512 KB      1.10%
```

#### 6. 文档
- ✅ `statemachine.md` - 完整的架构和使用指南
  - 分层结构说明
  - API 文档
  - 设计模式
  - 常见应用场景
  - 快速参考

---

## 📁 项目新增文件结构

```
F407VET6_Templet/
├── Core/
│   ├── Framework/
│   │   ├── Inc/
│   │   │   ├── system_tick.h
│   │   │   ├── task_scheduler.h
│   │   │   ├── state_machine.h
│   │   │   └── device_base.h
│   │   └── Src/
│   │       ├── system_tick.c
│   │       ├── task_scheduler.c
│   │       └── state_machine.c
│   ├── Drivers/
│   │   ├── Inc/
│   │   │   └── motor_driver.h
│   │   └── Src/
│   │       └── motor_driver.c
│   └── App/
│       ├── Inc/
│       │   └── app_config.h
│       └── Src/
│           └── app_config.c
├── statemachine.md
└── ARCHITECTURE_SUMMARY.md (此文件)
```

---

## 🏗️ 分层架构

```
┌─────────────────────────────────────────────┐
│ 应用层 (Application)                        │
│ - 循迹、避障等业务逻辑                      │
│ - 使用状态机 + 任务调度器                   │
│ 文件: Core/App/                             │
└─────────────────────────────────────────────┘
          ↑
          │ 调用
          ↓
┌─────────────────────────────────────────────┐
│ 设备驱动层 (Device Drivers)                 │
│ - 电机、传感器等具体设备                    │
│ - 虚函数指针表实现多态                      │
│ 文件: Core/Drivers/                         │
└─────────────────────────────────────────────┘
          ↑
          │ 使用
          ↓
┌─────────────────────────────────────────────┐
│ 框架层 (Framework)                          │
│ - 任务调度器（周期任务）                    │
│ - 状态机框架（有限状态机）                  │
│ - 系统滴答（1ms 中断）                      │
│ - 设备基类（接口定义）                      │
│ 文件: Core/Framework/                       │
└─────────────────────────────────────────────┘
          ↑
          │ 调用
          ↓
┌─────────────────────────────────────────────┐
│ HAL 适配层 (Abstraction)                    │
│ - CubeMX 生成的初始化                       │
│ - GPIO、时钟、中断配置                      │
│ 文件: Core/Src/                             │
└─────────────────────────────────────────────┘
          ↑
          │ 使用
          ↓
┌─────────────────────────────────────────────┐
│ 驱动层 (Drivers - 厂商提供)                 │
│ - STM32 HAL 库                              │
│ - CMSIS 库                                  │
│ 文件: Drivers/                              │
└─────────────────────────────────────────────┘
```

---

## 🔄 核心设计模式

### 1. 虚函数指针模式（多态）

```c
// 定义操作接口
typedef struct {
    int (*set_speed)(void *dev, uint16_t speed);
    int (*set_direction)(void *dev, uint8_t dir);
} MotorOps_t;

// 具体对象包含操作表
typedef struct {
    MotorOps_t *ops;  // 虚函数表
    uint16_t pwm_value;
} Motor_t;

// 使用时调用虚函数
motor.ops->set_speed(&motor, 500);
```

**优势**：支持设备替换、代码复用、易于扩展

### 2. 手动分频计数器（任务调度）

```
每次 scheduler_tick()（每 1ms）：
  For each task:
    if (enabled):
      tick_count++
      if (tick_count >= period_ms):
        tick_count = 0
        task_func()  // 执行任务
```

**特点**：
- 无需 RTOS，开销低
- 适合简单的周期性任务
- 易于调试

### 3. 状态机模式（有限状态机）

```
enter() ─→ 进入状态时调用一次（初始化）
process() ─→ 每次更新时调用（执行逻辑）
exit() ─→ 离开状态时调用一次（清理）
```

**使用场景**：循迹、避障、多设备协调等复杂工作流

### 4. ISR 快速返回模式（中断安全）

```c
// ISR 中仅设置标志
void ISR(void) {
    flag = 1;  // 快速返回
}

// 任务中处理
void task(void) {
    if (flag) {
        flag = 0;
        process_data();  // 长操作
    }
}
```

---

## 🚀 快速开始

### 1. 注册任务

```c
void app_init(void) {
    scheduler_add_task(read_sensors, 10);      // 10ms
    scheduler_add_task(motor_control, 5);      // 5ms
    scheduler_add_task(main_algorithm, 50);    // 50ms
}
```

### 2. 创建状态机

```c
// 定义状态处理器
static const StateHandler_t handlers[NUM_STATES] = {
    [STATE_INIT] = {
        .enter = state_init_enter,
        .process = state_init_process,
        .exit = state_init_exit,
    },
    // ... 其他状态
};

// 初始化
StateMachine_t fsm;
state_machine_init(&fsm, handlers, NUM_STATES, STATE_INIT);

// 更新
state_machine_update(&fsm);
```

### 3. 创建设备驱动

```c
Motor_t motor;
motor_init(&motor, pwm_handle, pwm_channel, gpio_port, pin_fwd, pin_bwd);
motor.ops->set_speed(&motor, 500);
motor.ops->set_direction(&motor, MOTOR_FORWARD);
```

---

## 📊 编译验证结果

```bash
$ cmake --preset Debug
-- Configuring done (15.2s)
-- Generating done (0.1s)

$ cmake --build build/Debug
[27/27] Linking C executable F407VET6_Templet.elf

Memory region         Used Size  Region Size  %age Used
             RAM:        1712 B       128 KB      1.31%
          CCMRAM:           0 B        64 KB      0.00%
           FLASH:        5784 B       512 KB      1.10%
```

✅ **编译成功，无错误和警告**

---

## 📚 文档导航

| 文档 | 内容 |
|------|------|
| `CLAUDE.md` | 项目总体指南、规范 |
| `statemachine.md` | 架构详解、API 文档、设计模式、使用示例 |
| `Core/Framework/Inc/*.h` | 框架 API 注释文档 |
| `Core/Drivers/Inc/*.h` | 设备驱动接口文档 |

---

## 🔧 后续开发建议

### 短期（必做）
1. 在 STM32CubeMX 中启用所需外设（UART、SPI、I2C、ADC 等）
2. 为每个硬件模块创建驱动 (参考 `motor_driver.c` 模板)
3. 在 `Core/App/` 中实现具体的业务逻辑

### 中期（推荐）
1. 实现 ISR 中的快速返回模式
2. 添加错误处理和日志输出
3. 进行硬件集成测试

### 长期（优化）
1. 性能分析和优化
2. 功耗管理
3. 故障诊断和恢复机制

---

## ✅ 验收清单

- ✅ 框架层完整实现
- ✅ 设备驱动示例（电机）
- ✅ 应用层骨架
- ✅ 主程序集成
- ✅ 编译成功，无错误
- ✅ 完整文档

---

## 📝 注意事项

1. **中断安全**：ISR 中仅设置标志，具体处理在任务中进行
2. **栈使用**：避免在 ISR 中分配大对象，使用静态或全局变量
3. **任务周期**：选择合理的周期避免 CPU 饱和（通常 < 70%）
4. **状态转移**：状态变化在下次 `update()` 时生效

---

**架构设计完成！项目已准备就绪进行实际硬件开发。**

