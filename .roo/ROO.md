# ROO.md — Roo Code 项目指南

本文件为 Roo Code 提供在仓库中工作的指导。

## 项目概览

这是一个 **STM32F407VET6** 微控制器模板项目，专为电子设计竞赛（电赛）设计，
采用**三层架构**（设备层 · 框架层 · 应用层）。

- **目标芯片**: STM32F407VET6 (ARM Cortex-M4 with FPU, 168MHz)
- **配置工具**: STM32CubeMX ([`F407VET6_Templet.ioc`](../F407VET6_Templet.ioc))
- **构建系统**: CMake + Ninja
- **调试工具**: OpenOCD + Cortex-Debug (VS Code)
- **编译器**: arm-none-eabi-gcc (C11 标准)

### 三层架构

1. **设备层** ([`Devices/`](../Devices/)) — 基于 C 语言面向对象（虚函数表）的硬件抽象
2. **框架层** ([`Framework/`](../Framework/)) — 通信系统、运动控制、状态机（开发中）
3. **应用层** ([`Application/`](../Application/)) — 业务逻辑、回调函数、测试程序

### 已规划硬件模块

| 模块 | 接口 | 驱动 | 备注 |
|------|------|------|------|
| DC 有刷电机 | PWM+方向 | TB6612/TB6600 | 双通道，最大 1.2A/通道 |
| MPU6050 IMU | I2C | I2C | 地址 0x68/0x69 |
| 8 路灰度循迹模块 | ADC/GPIO | GPIO/ADC | 1-3cm 检测距离 |
| 28BYJ-48 步进电机 | 4 相 GPIO | ULN2003 | 2048 步/转 |
| K230 视觉模块 | UART/Python | API/UART | Python 实现 |
| 编码器 | Timer | Timer | 速度/位置反馈 |
| 舵机 | PWM | PWM | 标准舵机控制 |

## 构建命令

```bash
# Debug 配置
cmake --preset Debug

# Debug 构建
cmake --build build/Debug

# Release 配置
cmake --preset Release
cmake --build build/Release
```

构建产物位于 `build/Debug/F407VET6_Templet.elf`

## 项目目录结构（实际）

```
F407VET6_Templet/
├── Core/                          # STM32CubeMX 生成代码
│   ├── Inc/                       #   main.h, gpio.h, dma.h, tim.h, usart.h ...
│   └── Src/                       #   main.c, gpio.c, dma.c, tim.c, usart.c ...
│
├── Drivers/                       # STM32 HAL & CMSIS 库（厂商提供）
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
│
├── Devices/                       # 设备层 — 硬件抽象
│   ├── Inc/DeviceClass/
│   │   ├── Modules/               #   ModuleBase.h（基类）, LED.h
│   │   ├── Motors/                #   （预留）
│   │   └── Sensors/               #   （预留）
│   ├── Inc/DebugPeripheral/       #   （预留）
│   ├── Inc/FilterAlgorithm/       #   （预留）
│   ├── Src/DeviceClass/Modules/   #   ModuleBase.c, LED.c
│   ├── Src/DeviceClass/Motors/    #   （预留）
│   ├── Src/DeviceClass/Sensors/   #   （预留）
│   ├── Src/DebugPeripheral/       #   （预留）
│   └── Src/FilterAlgorithm/       #   （预留）
│
├── Application/                   # 应用层 — 业务逻辑
│   ├── Inc/
│   │   ├── Callback.h             #   中断回调声明
│   │   └── TestProgram/           #   LedTest.h
│   └── Src/
│       ├── Callback.c             #   中断回调实现
│       └── TestProgram/           #   LedTest.c
│
├── Framework/                     # 框架层（开发中）
│                                 #   预留：通信系统、运动控制、状态机
│
├── cmake/                         # 构建配置
│   ├── gcc-arm-none-eabi.cmake
│   ├── starm-clang.cmake
│   └── stm32cubemx/CMakeLists.txt
│
├── docs/                          # 项目文档
│   ├── htmls/                     #   HTML 参考（定时器指南）
│   └── mds/                       #   技术文档（MPU6050 参数, PID 调参）
│
├── .claude/                       # Claude Code 配置
├── .roo/                          # Roo Code 配置（← 当前文件所在）
│
├── F407VET6_Templet.ioc           # CubeMX 项目文件
├── CMakeLists.txt                 # CMake 构建文件
├── CMakePresets.json              # CMake 预设配置
├── startup_stm32f407xx.s          # 启动文件
├── STM32F407XX_FLASH.ld           # 链接脚本
│
├── CLAUDE.md                      # 项目技术指南
├── ARCHITECTURE_SUMMARY.md        # 架构实现总结
├── README.md                      # 项目概述（三层架构）
├── README_CLAUDE.md               # Claude Code 快速开始
├── statemachine.md                # 状态机 & 架构设计指南
└── Simulation.md                  # C 面向对象模拟指南
```

## 开发规范

### 初始化顺序
1. `SystemClock_Config()` — 时钟配置
2. `MX_GPIO_Init()` — GPIO 初始化
3. `MX_<PERIPHERAL>_Init()` — 外设初始化
4. `HAL_NVIC_EnableIRQ()` — 中断启用

### 关键规则
- 始终检查 HAL 返回值: `if (HAL_<Func>(...) != HAL_OK) Error_Handler();`
- ISR 必须快速执行，仅设标志位，主循环处理逻辑
- `volatile` 修饰 ISR 与主程序共享的变量
- 避免动态内存分配，使用静态/栈分配
- DMA 缓冲区必须为全局或静态变量

### USER CODE 保护
CubeMX 生成的文件使用 `/* USER CODE BEGIN/END */` 标记保护用户代码。
任何自定义代码必须放在这些标记之间，否则将在重新生成时被覆盖。

### 添加新设备驱动
1. 在 [`Devices/Inc/DeviceClass/<Category>/`](../Devices/Inc/DeviceClass/) 创建头文件（继承 ModuleBase）
2. 在 [`Devices/Src/DeviceClass/<Category>/`](../Devices/Src/DeviceClass/) 创建源文件
3. 在 [`CMakeLists.txt`](../CMakeLists.txt) 注册源文件和头文件路径

## 相关文档

- 详见 `.roo/rules/` 开发规范：
  - [`embedded-best-practices.md`](rules/embedded-best-practices.md) — 嵌入式开发最佳实践
  - [`motor-control-guide.md`](rules/motor-control-guide.md) — 电机控制完整指南
  - [`sensor-modules-guide.md`](rules/sensor-modules-guide.md) — 传感器模块指南
  - [`hardware-integration.md`](rules/hardware-integration.md) — 硬件集成清单
  - [`schematic-reading-guide.md`](rules/schematic-reading-guide.md) — 原理图解读
  - [`code-review-checklist.md`](rules/code-review-checklist.md) — 代码审查清单
- 详见 `.roo/memory/` 硬件参考：
  - [`real-hardware-modules.md`](memory/real-hardware-modules.md) — 实际硬件清单
  - [`quick-i2c-ref.md`](memory/quick-i2c-ref.md) — MPU6050 I2C 快速参考
  - [`motor-params.md`](memory/motor-params.md) — 电机参数详细文档
- 详见 `.roo/skills/` 技能指南：
  - [`build-and-debug.md`](skills/build-and-debug.md) — 构建与调试
  - [`create-STM32-module.md`](skills/create-STM32-module.md) — 创建新模块
  - [`mpu6050-setup.md`](skills/mpu6050-setup.md) — MPU6050 设置
  - [`stepper-motor-28byj48.md`](skills/stepper-motor-28byj48.md) — 步进电机控制
  - [`tb6612-motor-control.md`](skills/tb6612-motor-control.md) — TB6612 电机控制
- 详见 [`docs/mds/`](../docs/mds/) 文档（MPU6050 参数、PID 调参经验）
- 详见 [`statemachine.md`](../statemachine.md) 架构与状态机设计指南

---

> 本文档与 `CLAUDE.md` 同步，保持项目结构描述一致。
