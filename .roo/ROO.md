# ROO.md — Roo Code 项目指南

本文件为 Roo Code 提供在仓库中工作的指导。

## 项目概览

这是一个 **STM32F407VET6** 微控制器项目，使用 STM32CubeMX 进行硬件配置，CMake + Ninja 进行构建。

- **目标芯片**: STM32F407VET6 (ARM Cortex-M4 with FPU)
- **配置工具**: STM32CubeMX (`.ioc` 文件)
- **构建系统**: CMake + Ninja
- **调试工具**: OpenOCD + Cortex-Debug (VS Code)
- **编译器**: arm-none-eabi-gcc (C11 标准)

### 已集成硬件模块

| 模块 | 接口 | 驱动 | 备注 |
|------|------|------|------|
| DC 有刷电机 | PWM+方向 | TB6612/TB6600 | 双通道，最大 1.2A/通道 |
| MPU6050 IMU | I2C | I2C | 地址 0x68/0x69 |
| 8 路灰度循迹模块 | ADC/GPIO | GPIO/ADC | 1-3cm 检测距离 |
| 28BYJ-48 步进电机 | 4 相 GPIO | ULN2003 | 2048 步/转 |

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

## 项目目录结构

```
├── Core/
│   ├── Inc/               # 头文件
│   ├── Src/               # 源代码 (main.c, gpio.c, usart.c, ...)
│   └── Src/main.c         # 主入口
├── Drivers/
│   ├── CMSIS/             # ARM CMSIS 库
│   └── STM32F4xx_HAL_Driver/  # STM32F4 HAL 驱动
├── cmake/                 # CMake 构建配置
├── .claude/               # Claude Code 配置
├── .roo/                  # Roo Code 配置 (← 当前文件所在)
└── F407VET6_Templet.ioc   # CubeMX 项目文件
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

### USER CODE 保护
CubeMX 生成的文件使用 `/* USER CODE BEGIN/END */` 标记保护用户代码。任何自定义代码必须放在这些标记之间，否则将在重新生成时被覆盖。

## 相关文档

- 详见 `.roo/rules/embedded-best-practices.md` — 嵌入式开发最佳实践
- 详见 `.roo/memory/real-hardware-modules.md` — 实际硬件清单
- 详见 `.roo/memory/quick-i2c-ref.md` — MPU6050 I2C 快速参考
- 详见 `.roo/memory/motor-params.md` — 电机参数详细文档

---

> 本文档由 `.claude/CLAUDE.md` 移植而来，适配 Roo Code 工作流。
