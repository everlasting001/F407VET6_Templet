# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an **STM32F407VET6** microcontroller template project for electronics design competitions (电赛). The project follows a **three-layer architecture**:

1. **Device Layer** ([`Devices/`](Devices/)) — Hardware abstraction via C-based OOP (虚函数表)
2. **Framework Layer** ([`Framework/`](Framework/)) — Communication, motion control, state machine (under development)
3. **Application Layer** ([`Application/`](Application/)) — Business logic, callbacks, test programs

**Target**: STM32F407VET6 (ARM Cortex-M4 with FPU)  
**Configuration Tool**: STM32CubeMX (`.ioc` file)  
**Build System**: CMake with Ninja generator  
**Debugging**: OpenOCD + Cortex-Debug extension in VS Code  
**Compiler**: arm-none-eabi-gcc (C11 standard)

### Supported Hardware Modules (Planned)

| Module | Interface | Driver | Notes |
|--------|-----------|--------|-------|
| DC Brushed Motor | PWM+GPIO | TB6612/TB6600 | Dual channel, max 1.2A/ch |
| MPU6050 IMU | I2C | I2C | Address 0x68/0x69 |
| 8-Ch Grayscale Sensor | ADC/GPIO | GPIO/ADC | 1-3cm detection |
| 28BYJ-48 Stepper Motor | 4-phase GPIO | ULN2003 | 2048 steps/rev |
| Vision Sensor (K230) | UART/Python | API/UART | Python-based |
| Encoder | Timer | Timer | For speed/pos feedback |
| Servo | PWM | PWM | Standard servo control |

## Build & Development

### Building the Project

Use CMake with one of the provided presets:

```bash
# Configure Debug build
cmake --preset Debug

# Build Debug version
cmake --build build/Debug

# Or configure Release build
cmake --preset Release
cmake --build build/Release
```

The compiled ELF file will be at `build/Debug/F407VET6_Templet.elf` (or `build/Release/` for Release builds).

### Quick Compile Check

```bash
cmake --build build/Debug 2>&1 | findstr /I "error warning"
```

## Project Structure (Actual)

```
F407VET6_Templet/
├── Core/                          # STM32CubeMX generated code
│   ├── Inc/                       #   HAL headers (dma.h, gpio.h, main.h, tim.h, usart.h, ...)
│   └── Src/                       #   HAL sources (main.c, gpio.c, dma.c, tim.c, usart.c, ...)
│
├── Drivers/                       # STM32 HAL & CMSIS libraries (vendor)
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
│
├── Devices/                       # Device Layer — hardware abstraction
│   ├── Inc/
│   │   ├── DeviceClass/
│   │   │   ├── Modules/           #   Base: ModuleBase.h, LED.h
│   │   │   ├── Motors/            #   (future: DCMotor, StepMotor, Servo)
│   │   │   └── Sensors/           #   (future: Encoder, Gyro, LineSensor, VisionSensor)
│   │   ├── DebugPeripheral/       #   (future: UART debug, OLED, SPI, I2C)
│   │   └── FilterAlgorithm/       #   (future: PID, sensor filters)
│   └── Src/
│       ├── DeviceClass/Modules/   #   ModuleBase.c, LED.c
│       ├── DeviceClass/Motors/
│       ├── DeviceClass/Sensors/
│       ├── DebugPeripheral/
│       └── FilterAlgorithm/
│
├── Application/                   # Application Layer — business logic
│   ├── Inc/
│   │   ├── Callback.h             #   Interrupt callback declarations
│   │   └── TestProgram/           #   LedTest.h
│   └── Src/
│       ├── Callback.c             #   Interrupt callback implementations
│       └── TestProgram/           #   LedTest.c
│
├── Framework/                     # Framework Layer (under development)
│                                 #   Future: Communication, Motion Control, State Machine
│
├── cmake/                         # CMake build configuration
│   ├── gcc-arm-none-eabi.cmake    #   ARM GCC toolchain
│   ├── starm-clang.cmake          #   Clang toolchain (optional)
│   └── stm32cubemx/               #   CubeMX-generated CMake lists
│
├── docs/                          # Project documentation
│   ├── htmls/                     #   HTML references (e.g., timer guide)
│   └── mds/                       #   Markdown docs (MPU6050 params, PID tuning)
│
├── .claude/                       # Claude Code AI configuration
├── .roo/                          # Roo Code AI configuration
├── .vscode/                       # VS Code IDE settings
├── .settings/                     # STM32CubeIDE settings
│
├── F407VET6_Templet.ioc           # STM32CubeMX project file
├── CMakeLists.txt                 # Root CMake build file
├── CMakePresets.json              # CMake presets (Debug/Release)
├── startup_stm32f407xx.s          # Startup assembly
├── STM32F407XX_FLASH.ld           # Linker script
│
├── CLAUDE.md                      # This file
├── README.md                      # Project overview (three-layer architecture)
├── ARCHITECTURE_SUMMARY.md        # Architecture implementation summary
├── README_CLAUDE.md               # Claude Code quick-start guide
├── statemachine.md                # State machine & architecture guide
└── Simulation.md                  # OOP simulation guide (C-based)
```

## Device Layer — C-based OOP

The Device Layer uses C to simulate object-oriented programming via virtual function tables (虚函数表):

```c
// ModuleBase.h — Base class with VTable
typedef struct {
    int (*init)(void *self);
    int (*run)(void *self);
    int (*cleanup)(void *self);
    void (*reset)(void *self);
} ModuleVTable;

typedef struct {
    ModuleVTable *vtable;
    uint8_t initialized;
    // ... common fields
} ModuleBase;
```

### Current Implementations

- [`ModuleBase`](Devices/Inc/DeviceClass/Modules/ModuleBase.h) — Abstract base class with init/run/cleanup/reset interface
- [`LED`](Devices/Inc/DeviceClass/Modules/LED.h) — Concrete LED module inheriting from ModuleBase

### Planned Implementations (per README.md)

| Category | Modules |
|----------|---------|
| **Modules** | KEY, OLED, BUZZER |
| **Sensors** | Encoder, Gyro (MPU6050), LineSensor (Grayscale), VisionSensor (K230) |
| **Motors** | DCMotor (TB6612), StepMotor (28BYJ-48), Servo |
| **Debug Peripheral** | UART debug, OLED print, SPI debug, I2C debug |
| **Filter Algorithm** | PID, sensor filtering algorithms |

## Adding User Code

### CubeMX-Generated Files (`Core/`)

Place custom code within `/* USER CODE BEGIN ... */` and `/* USER CODE END ... */` markers. Code outside these sections will be overwritten on regeneration.

### Custom Device Classes

Add new device source files to [`Devices/Src/DeviceClass/`](Devices/Src/DeviceClass/) (organized by category: Modules/, Motors/, Sensors/) and headers to [`Devices/Inc/DeviceClass/`](Devices/Inc/DeviceClass/).

### Application Code

Add application logic files to [`Application/Src/`](Application/Src/) and [`Application/Inc/`](Application/Inc/).

### CMake Registration

Update [`CMakeLists.txt`](CMakeLists.txt) to add new source files in `target_sources()` and include paths in `target_include_directories()`.

## STM32CubeMX Workflow

1. Modify `F407VET6_Templet.ioc` with STM32CubeMX
2. CubeMX regenerates code in `Core/` and `cmake/stm32cubemx/`
3. Preserve user code within `USER CODE BEGIN/END` markers
4. Re-run CMake configure after regeneration

## Debugging

Configured for OpenOCD + Cortex-Debug in VS Code:
- **Launch config**: [`.vscode/launch.json`](.vscode/launch.json)
- **Debugger**: arm-none-eabi-gdb
- **Interface**: CMSIS-DAP (via ST-LINK or compatible probe)
- **Config files**: `interface/cmsis-dap.cfg`, `target/stm32f4x.cfg`

```bash
# Terminal 1: Start OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg

# Terminal 2: VS Code F5 (or use launch config)
```

## Embedded Development Standards

### HAL Initialization Order (enforced in generated main.c)

1. `SystemClock_Config()` — Clock configuration
2. `MX_GPIO_Init()` — GPIO setup
3. `MX_<PERIPHERAL>_Init()` — UART, SPI, I2C, ADC, etc.
4. `HAL_NVIC_EnableIRQ()` — Interrupt after all peripherals

### Critical Rules

- Always check HAL return values: `if (HAL_<Func>(...) != HAL_OK) Error_Handler();`
- ISRs must be **fast** — set flags, let main loop handle logic
- Use `volatile` for variables shared between ISR and main
- Avoid `malloc`/`free` — use static arrays or stack allocation
- DMA buffers must be **global or static** (never on stack)

### Code Organization

- Generated code stays in `Core/`
- Custom device drivers go in `Devices/`
- Application logic goes in `Application/`
- Framework components go in `Framework/`
- Never modify `Drivers/` (vendor HAL library)

## Toolchain

- **Compiler**: arm-none-eabi-gcc
- **C Standard**: C11 (required by generated code)
- **Linker Script**: [`STM32F407XX_FLASH.ld`](STM32F407XX_FLASH.ld)
- **Startup**: [`startup_stm32f407xx.s`](startup_stm32f407xx.s)

## Known Configuration

- HAL drivers enabled with `-DUSE_HAL_DRIVER`
- Debug configuration includes `-DDEBUG` in Debug builds
- Clangd configured in [`.clangd`](.clangd) for VS Code IntelliSense

## Related Documentation

- See `.roo/rules/` or `.claude/rules/` for development guidelines:
  - [`embedded-best-practices.md`](.roo/rules/embedded-best-practices.md) — Embedded C best practices
  - [`motor-control-guide.md`](.roo/rules/motor-control-guide.md) — TB6612 & 28BYJ-48 control
  - [`sensor-modules-guide.md`](.roo/rules/sensor-modules-guide.md) — MPU6050, grayscale sensor
  - [`hardware-integration.md`](.roo/rules/hardware-integration.md) — Hardware integration checklist
  - [`schematic-reading-guide.md`](.roo/rules/schematic-reading-guide.md) — Schematic reading
  - [`code-review-checklist.md`](.roo/rules/code-review-checklist.md) — Code review checklist
- See [`.claude/memory/`](.claude/memory/) for hardware reference:
  - [`real-hardware-modules.md`](.claude/memory/real-hardware-modules.md) — Hardware inventory
  - [`quick-i2c-ref.md`](.claude/memory/quick-i2c-ref.md) — MPU6050 I2C register map
  - [`motor-params.md`](.claude/memory/motor-params.md) — Motor parameters
- See [`.claude/docs/`](.claude/docs/) for datasheets and schematics
- See [`docs/mds/`](docs/mds/) for MPU6050 parameters and PID tuning experience
- See [`statemachine.md`](statemachine.md) for architecture & state machine design guide

## Development Workflow

1. Configure peripherals in STM32CubeMX (`.ioc` file)
2. Generate code → files in `Core/`
3. Implement device drivers in `Devices/DeviceClass/`
4. Write application logic in `Application/`
5. Register new source files in [`CMakeLists.txt`](CMakeLists.txt)
6. Build: `cmake --build build/Debug`
7. Debug with OpenOCD + GDB (VS Code F5)
