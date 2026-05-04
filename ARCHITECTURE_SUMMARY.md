# STM32F407VET6 分层架构实现总结

**完成日期**: 2026-04-25  
**编译状态**: ✅ 成功 (Debug 配置)  
**目标**: 为电赛项目建立规范化的分层架构和任务调度系统

---

## 📋 实现清单

### ✅ 已完成

#### 1. 设备驱动基类 ([`Devices/Inc/DeviceClass/Modules/`](Devices/Inc/DeviceClass/Modules/))
- ✅ [`ModuleBase.h`](Devices/Inc/DeviceClass/Modules/ModuleBase.h) — 虚函数表基类（init/run/cleanup/reset）
- ✅ [`ModuleBase.c`](Devices/Src/DeviceClass/Modules/ModuleBase.c) — 基类实现
- ✅ [`LED.h`](Devices/Inc/DeviceClass/Modules/LED.h) — LED 模块头文件（继承 ModuleBase）
- ✅ [`LED.c`](Devices/Src/DeviceClass/Modules/LED.c) — LED 模块实现

**特性**：
- 虚函数指针表实现 C 语言面向对象多态
- 统一设备接口（init/run/cleanup/reset）
- 可扩展的模块化设计

#### 2. 应用层骨架 ([`Application/`](Application/))
- ✅ [`Callback.h`](Application/Inc/Callback.h) / [`Callback.c`](Application/Src/Callback.c) — 中断回调函数集中管理
- ✅ [`LedTest.c`](Application/Src/TestProgram/LedTest.c) / [`LedTest.h`](Application/Inc/TestProgram/LedTest.h) — LED 测试程序

#### 3. 主程序集成
- ✅ [`Core/Src/main.c`](Core/Src/main.c) — CubeMX 生成的主入口
- ✅ [`CMakeLists.txt`](CMakeLists.txt) — 已注册 Devices/ 和 Application/ 源文件

**CMake 编译验证**：
```
Memory region         Used Size  Region Size  %age Used
             RAM:        1712 B       128 KB      1.31%
          CCMRAM:           0 B        64 KB      0.00%
           FLASH:        5784 B       512 KB      1.10%
```

#### 4. 文档
- ✅ [`CLAUDE.md`](CLAUDE.md) — 项目技术指南（已更新同步）
- ✅ [`statemachine.md`](statemachine.md) — 架构详解、API 文档、设计模式、使用示例
- ✅ [`ARCHITECTURE_SUMMARY.md`](ARCHITECTURE_SUMMARY.md) — 本文件
- ✅ [`Simulation.md`](Simulation.md) — C 面向对象模拟学习指南

### 🚧 开发中 / 预留目录

| 分层 | 路径 | 状态 |
|------|------|------|
| **设备层 — Motors** | `Devices/Inc/DeviceClass/Motors/` | 📁 预留（DCMotor, StepMotor, Servo） |
| **设备层 — Sensors** | `Devices/Inc/DeviceClass/Sensors/` | 📁 预留（Encoder, Gyro, LineSensor, VisionSensor） |
| **设备层 — DebugPeripheral** | `Devices/Inc/DebugPeripheral/` | 📁 预留（UART, OLED, SPI, I2C debug） |
| **设备层 — FilterAlgorithm** | `Devices/Inc/FilterAlgorithm/` | 📁 预留（PID, 传感器滤波） |
| **框架层 — 通信系统** | `Framework/` | 📁 预留（UART 通信） |
| **框架层 — 运动控制** | `Framework/` | 📁 预留（PID 控制器） |
| **框架层 — 状态机** | `Framework/` | 📁 预留（状态机系统） |
| **应用层 — 主程序** | `Application/Inc/` | 📁 预留 |
| **应用层 — 任务集成** | `Application/Inc/` | 📁 预留 |

---

## 🏗️ 三层架构（按 README.md）

```
┌─────────────────────────────────────────────┐
│ 应用层 (Application Layer)                  │
│ - 主程序、回调函数、测试程序、任务集成       │
│ - 文件: Application/                        │
└─────────────────────────────────────────────┘
          ↑ 调用
┌─────────────────────────────────────────────┐
│ 框架层 (Framework Layer)                    │
│ - 通信系统、运动控制、状态机                │
│ - 文件: Framework/（开发中）                │
└─────────────────────────────────────────────┘
          ↑ 使用
┌─────────────────────────────────────────────┐
│ 设备层 (Device Layer)                       │
│ - 设备类（Modules/Sensors/Motors）          │
│ - 调试外设、滤波算法                        │
│ - 文件: Devices/                            │
└─────────────────────────────────────────────┘
          ↑ 调用硬件
┌─────────────────────────────────────────────┐
│ HAL 适配层 (CubeMX Generated)               │
│ - CubeMX 生成的初始化代码                   │
│ - GPIO、时钟、中断、外设配置                │
│ - 文件: Core/                               │
└─────────────────────────────────────────────┘
          ↑ 使用
┌─────────────────────────────────────────────┐
│ 驱动层 (Vendor HAL Library)                 │
│ - STM32 HAL 库 + CMSIS 库                   │
│ - 文件: Drivers/                            │
└─────────────────────────────────────────────┘
```

---

## 🔄 核心设计模式

### 1. 虚函数指针模式（多态）

```c
// ModuleBase.h — 定义虚函数表
typedef struct {
    int (*init)(void *self);
    int (*run)(void *self);
    int (*cleanup)(void *self);
    void (*reset)(void *self);
} ModuleVTable;

typedef struct {
    ModuleVTable *vtable;
    uint8_t initialized;
    // ... 子类可扩展字段
} ModuleBase;

// 子类使用
typedef struct {
    ModuleBase base;       // 继承基类
    // LED 特有字段
    uint16_t gpio_pin;
    GPIO_TypeDef *port;
} LED;
```

### 2. 目录组织与分层

```
Devices/                     ← 设备层
├── Inc/DeviceClass/
│   ├── Modules/             ← 基础模块（ModuleBase, LED, KEY, OLED, BUZZER）
│   ├── Sensors/             ← 传感器（Encoder, Gyro, LineSensor, VisionSensor）
│   └── Motors/              ← 电机（DCMotor, StepMotor, Servo）
├── Inc/DebugPeripheral/     ← 调试外设（UART, OLED, SPI, I2C）
├── Inc/FilterAlgorithm/     ← 滤波算法（PID, 传感器滤波）
└── Src/                     ← 对应的源文件

Application/                 ← 应用层
├── Inc/
│   ├── Callback.h           ← 中断回调声明
│   └── TestProgram/         ← 测试程序
└── Src/
    ├── Callback.c           ← 中断回调实现
    └── TestProgram/         ← 测试程序实现
```

---

## 🚀 快速开始

### 1. 创建新设备（以 KEY 为例）

```c
// Devices/Inc/DeviceClass/Modules/KEY.h
#include "ModuleBase.h"

typedef struct {
    ModuleBase base;
    uint16_t gpio_pin;
    GPIO_TypeDef *port;
} KEY;

// 实现虚函数表
int KEY_init(void *self);
int KEY_run(void *self);
int KEY_cleanup(void *self);
void KEY_reset(void *self);

extern ModuleVTable KEY_vtable;
```

### 2. 在 CMakeLists.txt 注册

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Devices/Src/DeviceClass/Modules/ModuleBase.c
    Devices/Src/DeviceClass/Modules/LED.c
    Devices/Src/DeviceClass/Modules/KEY.c        # 新增
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Devices/Inc/DeviceClass/Modules
)
```

---

## 📚 文档导航

| 文档 | 内容 |
|------|------|
| [`README.md`](README.md) | 项目概述、三层架构定义 |
| [`CLAUDE.md`](CLAUDE.md) | 项目总体指南、构建规范 |
| [`statemachine.md`](statemachine.md) | 架构详解、API 文档、设计模式、使用示例 |
| [`Simulation.md`](Simulation.md) | C 面向对象模拟学习笔记 |
| [`.claude/docs/README.md`](.claude/docs/README.md) | 硬件文档索引 |
| [`.claude/rules/`](.claude/rules/) | 开发规范（最佳实践、审查清单、电机指南等） |

---

## 🔧 后续开发建议

### 短期（必做）
1. 在 STM32CubeMX 中启用所需外设（UART、SPI、I2C、ADC 等）
2. 为每个硬件模块创建驱动（参考 `ModuleBase.h` 模板）
3. 在 `Application/` 中实现具体的业务逻辑

### 中期（推荐）
1. 完善框架层 `Framework/`（通信系统、运动控制、状态机）
2. 实现各类传感器驱动（`Devices/Inc/DeviceClass/Sensors/`）
3. 实现各类电机驱动（`Devices/Inc/DeviceClass/Motors/`）

### 长期（优化）
1. 性能分析和优化
2. 功耗管理
3. 故障诊断和恢复机制

---

## ✅ 验收清单

- ✅ 设备驱动基类（ModuleBase）完整实现
- ✅ LED 模块实现（继承 ModuleBase）
- ✅ 应用层骨架（回调 + 测试程序）
- ✅ CMake 构建配置
- ✅ 编译成功，无错误（RAM: 1712B, FLASH: 5784B）
- ✅ 完整文档

---

## 📝 注意事项

1. **中断安全**：ISR 中仅设置标志，具体处理在任务中进行（参见 `Application/Src/Callback.c`）
2. **USER CODE 保护**：所有对 `Core/` 中生成文件的修改必须放在 `/* USER CODE BEGIN/END */` 标记内
3. **新设备注册**：添加新设备时需在 `CMakeLists.txt` 注册源文件和头文件路径
4. **设备分层**：所有设备驱动必须继承 `ModuleBase`，实现统一的 init/run/cleanup/reset 接口

---

**架构设计完成！项目已准备就绪进行实际硬件开发。**
