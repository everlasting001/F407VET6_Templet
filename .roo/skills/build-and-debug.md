# Skill: STM32 构建与调试流程

## 用途
快速完成 STM32F407 项目的 CMake 配置、编译和 OpenOCD 调试流程。

## 使用方式
在 Roo Code 中通过 `skill("build-and-debug", "<动作>")` 调用。

## 可用动作

### `configure` — CMake 配置
```bash
cmake --preset Debug
```

### `build` — 编译项目
```bash
cmake --build build/Debug
```

### `clean` — 清理构建
```bash
cmake --build build/Debug --target clean
```

### `rebuild` — 重新配置并构建
```bash
cmake --preset Debug && cmake --build build/Debug
```

### `quick-check` — 快速编译检查
```bash
cmake --build build/Debug 2>&1 | findstr /I "error warning"
```

## 调试

### 启动 OpenOCD
```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg
```

### GDB 连接
```bash
arm-none-eabi-gdb build/Debug/F407VET6_Templet.elf -ex "target remote localhost:3333"
```

## 构建产物
- ELF: `build/Debug/F407VET6_Templet.elf`
- MAP: `build/Debug/F407VET6_Templet.map`
- HEX: `build/Debug/F407VET6_Templet.hex`
- BIN: `build/Debug/F407VET6_Templet.bin`
