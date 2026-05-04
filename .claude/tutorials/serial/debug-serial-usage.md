# STM32 调试串口（Debug UART）使用说明

## 目录

1. [概述](#1-概述)
2. [快速开始](#2-快速开始)
3. [硬件配置](#3-硬件配置)
4. [软件架构](#4-软件架构)
5. [初始化流程](#5-初始化流程)
6. [发送调试信息](#6-发送调试信息)
7. [调试级别控制](#7-调试级别控制)
8. [接收数据](#8-接收数据)
9. [printf 重定向原理](#9-printf-重定向原理)
10. [实战示例](#10-实战示例)
11. [常见问题排查](#11-常见问题排查)
12. [相关文件索引](#12-相关文件索引)

---

## 1. 概述

本项目提供一个**高性能调试串口**，基于 `USART1 + DMA + IDLE 空闲中断` 架构，具有以下特点：

| 特性 | 说明 |
|------|------|
| 波特率 | **115200** 8N1（标准调试速率） |
| 发送方式 | **DMA 非阻塞发送**，几乎不占用 CPU |
| 接收方式 | **DMA + IDLE 空闲中断**，不定长数据自动接收 |
| printf | 已重定向到 USART1，直接使用 `printf()` 即可输出 |
| 调试工具 | 提供带时间戳、hex dump 等专用调试函数 |
| 编译器支持 | ARM GCC (Newlib) 和 ARMCC (MDK-ARM) |

> 相比传统每字节中断方式，DMA + IDLE 可减少 **99% 的中断次数**，释放 CPU 处理核心控制算法。

---

## 2. 快速开始

### 2.1 接线

| STM32F407VET6 | USB-TTL 转换器 |
|:---:|:---:|
| **PA9** (USART1_TX) | RX |
| **PA10** (USART1_RX) | TX |
| **GND** | GND |

> 注意：TX 接 RX，RX 接 TX（交叉连接）。

### 2.2 PC 端串口终端配置

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | None |
| 流控制 | None |

推荐工具：PuTTY / MobaXterm / Serial Studio

### 2.3 代码中使用

```c
#include "usart.h"
#include "debug_utils.h"

// 1. 初始化（在 main 函数中）
MX_USART1_UART_Init();    // USART1 硬件初始化
uart_debug_init();        // 启动 DMA + IDLE 接收

// 2. 直接使用 printf 输出
printf("Hello, Debug World!\r\n");

// 3. 或使用带时间戳的调试输出
debug_printf("Sensor value: %d\r\n", value);

// 4. 或使用带级别的调试宏
DEBUG_INFO("System initialized\r\n");
```

---

## 3. 硬件配置

### 3.1 USART1 引脚

定义在 [`Core/Inc/main.h`](../../../Core/Inc/main.h) 中（由 CubeMX 生成）：

| 引脚 | 功能 | 复用模式 |
|------|------|---------|
| PA9 | USART1_TX | AF7 |
| PA10 | USART1_RX | AF7 |

### 3.2 DMA 资源

| 通道 | 方向 | 数据流 | 优先级 |
|------|------|--------|--------|
| DMA2_Stream2 | RX（外设→内存） | CH4 | Low |
| DMA2_Stream7 | TX（内存→外设） | CH4 | Low |

### 3.3 中断优先级

| 中断 | 优先级 |
|------|--------|
| USART1_IRQn | 0（最高） |
| DMA2_Stream2_IRQn | 0（最高） |
| DMA2_Stream7_IRQn | 0（最高） |

### 3.4 缓冲区大小

定义在 [`Core/Inc/usart.h`](../../../Core/Inc/usart.h) 中：

```c
// 接收缓冲区 — 接收来自 PC 的命令/数据
#define UART_DEBUG_RX_BUF_SIZE  256

// 发送缓冲区 — printf/debug_printf 格式化输出用
#define UART_DEBUG_TX_BUF_SIZE  512
```

> **建议**：如果需要高频数据输出（如陀螺仪 1kHz），将 TX 缓冲区增大到 1024 或 2048。

---

## 4. 软件架构

### 4.1 文件组织

```
Core/
├── Inc/
│   ├── usart.h              ← USART 初始化 + DMA+IDLE 函数声明
│   ├── callback.h           ← HAL 回调函数声明
│   └── debug_utils.h        ← 调试输出工具函数声明
└── Src/
│   ├── usart.c              ← MX_USART1_UART_Init() + DMA 收发实现
│   ├── callback.c           ← HAL 回调函数实现（核心）
│   ├── debug_utils.c        ← printf 重定向 + 格式化输出
│   └── main.c               ← 初始化入口
└── App/
    └── Src/
        └── app_config.c     ← 应用层处理接收数据（用户实现）
```

### 4.2 数据流程

```
发送（调试输出）：
  printf() / debug_printf() / debug_puts()
         │
         ▼
  debug_utils.c  →  vsnprintf 格式化
         │
         ▼
  usart.c  →  HAL_UART_Transmit_DMA()  →  PA9 输出

接收（PC→MCU）：
  PA10 输入  →  DMA 自动搬运到缓冲区
         │
         ▼
  IDLE 空闲中断触发
         │
         ▼
  callback.c  →  HAL_UARTEx_RxEventCallback()
         │
         ▼
  uart_debug_data_handler()  ← 用户自定义处理
```

---

## 5. 初始化流程

正确的初始化顺序至关重要，必须在 [`Core/Src/main.c`](../../../Core/Src/main.c) 中按以下顺序调用：

```c
int main(void)
{
    HAL_Init();                    // 1. HAL 库初始化
    SystemClock_Config();          // 2. 系统时钟 (168MHz)

    MX_GPIO_Init();                // 3. GPIO 初始化
    MX_DMA_Init();                 // 4. DMA 初始化（必须先于 USART！）
    MX_USART1_UART_Init();         // 5. USART1 初始化（配置 GPIO、DMA、中断）

    /* ========== 调试串口初始化 ========== */
    uart_debug_init();             // 6. ★ 启动 DMA + IDLE 接收
    /* ================================= */

    printf("System initialized successfully!\r\n");

    // 其他初始化...
    system_tick_init(1000);
    app_init();

    while (1) {
        app_main_loop();
    }
}
```

> ⚠️ 初始化 MX_USART1_UART_Init() 之前必须完成 MX_DMA_Init()，否则 DMA 配置无效。

---

## 6. 发送调试信息

项目提供多种调试输出方式，按需选择：

### 6.1 `printf()` — 标准输出

```c
#include <stdio.h>

printf("Count: %d, Value: %.2f\r\n", count, value);
```

- **优点**：标准函数，代码可移植性好
- **缺点**：无时间戳前缀
- **底层**：已重定向到 USART1（通过 `_write()` 或 `fputc()`）

### 6.2 `debug_printf()` — 带时间戳的格式化输出

```c
#include "debug_utils.h"

debug_printf("Gyro: %.2f %.2f %.2f\r\n", gx, gy, gz);
// 输出示例: [1.234] Gyro: 0.12 -0.45 9.81
```

- **优点**：自动添加 `[秒.毫秒]` 时间戳，方便时序分析
- **注意**：超过 `UART_DEBUG_TX_BUF_SIZE` 的消息会被截断

### 6.3 `debug_puts()` — 快速字符串输出

```c
debug_puts("Emergency stop!\r\n");
```

- **优点**：不经 vsnprintf 格式化，速度快、代码小
- **适用**：输出固定错误信息、状态字符串

### 6.4 `debug_hex_dump()` — 十六进制数据打印

```c
// 调试通信协议、传感器原始数据
uint8_t sensor_data[32];
debug_hex_dump("MPU", sensor_data, 32);
```

输出格式：
```
[MPU] 0000:  A1 B2 C3 D4 E5 F6 78 90  12 34 56 78 9A BC DE F0  |......x..4x.....|
[MPU] 0010:  01 02 03 04 05 06 07 08                           |........|
```

- **适用场景**：调试 I2C/SPI 协议数据、查看传感器原始寄存器值、分析通信报文

### 6.5 `uart_debug_send()` / `uart_debug_send_str()` — 底层发送

```c
uint8_t packet[] = {0xAA, 0xBB, 0x01, 0x02, 0xCC};
uart_debug_send(packet, 5);        // 发送原始二进制数据
uart_debug_send_str("Hello!\r\n"); // 发送字符串
```

- **适用**：需要发送二进制协议数据时直接调用
- **注意**：如果 DMA 忙，会自动降级为中断方式发送

---

## 7. 调试级别控制

通过 [`DEBUG_LEVEL`](../../../Core/Inc/debug_utils.h) 宏实现条件编译，**发布版本中自动移除调试代码**。

### 7.1 级别定义

| 宏 | 值 | 说明 |
|----|-----|------|
| `DEBUG_LEVEL_NONE` | 0 | 关闭所有调试输出 |
| `DEBUG_LEVEL_ERROR` | 1 | 仅输出错误信息 |
| `DEBUG_LEVEL_WARN` | 2 | 输出错误和警告 |
| `DEBUG_LEVEL_INFO` | 3 | （默认）输出一般信息 |
| `DEBUG_LEVEL_DEBUG` | 4 | 输出详细调试信息 |
| `DEBUG_LEVEL_VERBOSE` | 5 | 输出所有信息 |

### 7.2 使用示例

在编译选项中定义（推荐）或在 `debug_utils.h` 中修改：

```c
// 方式1：在 CMakeLists.txt 或编译选项中定义
// add_compile_definitions(DEBUG_LEVEL=DEBUG_LEVEL_DEBUG)

// 方式2：直接修改 debug_utils.h（临时调试）
// #define DEBUG_LEVEL DEBUG_LEVEL_DEBUG
```

代码中使用：

```c
DEBUG_ERROR("Motor overcurrent!\r\n");       // 始终输出
DEBUG_WARN("Battery low: %.1fV\r\n", volt);  // LEVEL >= 2 时输出
DEBUG_INFO("Task started\r\n");               // LEVEL >= 3 时输出（默认）
DEBUG_DEBUG("Register: 0x%02X\r\n", reg);    // LEVEL >= 4 时输出
```

> **最佳实践**：开发时设置 `DEBUG_LEVEL = DEBUG_LEVEL_VERBOSE`，发布时设置为 `DEBUG_LEVEL_ERROR` 或 `DEBUG_LEVEL_NONE`。

### 7.3 调试断言

```c
#include "debug_utils.h"

DEBUG_ASSERT(speed < MAX_SPEED, "Speed exceeded limit!");
// 条件不满足时输出: [ASSERT] Speed exceeded limit! at main.c:42
// 然后调用 Error_Handler()
```

---

## 8. 接收数据

### 8.1 默认回调处理

在 [`Core/Src/callback.c`](../../../Core/Src/callback.c) 中，`HAL_UARTEx_RxEventCallback()` 是 IDLE 中断触发后的核心回调：

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        uint8_t *rx_buf = uart_get_rx_buffer();

        // Size = 实际接收到的字节数
        // rx_buf = 接收缓冲区

        if (Size > 0) {
            uart_debug_data_handler(rx_buf, Size);  // 调用应用层处理
            memset(rx_buf, 0, Size);                // 清空缓冲区
        }

        // 重新启动 DMA 接收，准备下一帧
        HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, UART_DEBUG_RX_BUF_SIZE);
    }
}
```

### 8.2 自定义数据处理

通过实现弱函数 `uart_debug_data_handler()` 来处理接收数据。在任意 `.c` 文件中实现此函数即可覆盖默认实现：

```c
// 默认实现（在 callback.c 中）：回显接收到的数据
__attribute__((weak))
void uart_debug_data_handler(uint8_t *data, uint16_t len)
{
    uart_debug_send(data, len);  // 回显
}
```

用户自定义实现示例：

```c
// 在用户代码中重新实现，例如 app_config.c
void uart_debug_data_handler(uint8_t *data, uint16_t len)
{
    // 数据已被 IDLE 中断接收完毕，直接处理
    // data 指向接收缓冲区，len 是实际数据长度

    // 示例：简单命令解析
    if (strncmp((char*)data, "LED_ON", 6) == 0) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        debug_printf("LED turned ON\r\n");
    }
    else if (strncmp((char*)data, "LED_OFF", 7) == 0) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        debug_printf("LED turned OFF\r\n");
    }
    else {
        // 未知命令，回显
        uart_debug_send(data, len);
    }
}
```

### 8.3 轮询方式获取接收状态

也可在主循环中轮询接收标志：

```c
while (1) {
    if (uart_is_rx_ready()) {
        uint8_t *rx_data = uart_get_rx_buffer();
        uint16_t rx_len  = uart_get_last_rx_size();

        // 处理接收数据...

        uart_clear_rx_ready();  // 消费完成后清除标志
    }
    // 其他任务...
}
```

---

## 9. printf 重定向原理

### 9.1 ARM GCC (Newlib)

在 [`Core/Src/debug_utils.c`](../../../Core/Src/debug_utils.c) 中重写 `_write()` 系统调用：

```c
int _write(int file, char *ptr, int len)
{
    (void)file;
    if (ptr == NULL || len <= 0) return 0;

    uart_debug_send((uint8_t *)ptr, len);
    return len;
}
```

调用链：`printf("hello") → _write(1, "hello", 5) → uart_debug_send()`

### 9.2 ARMCC (MDK-ARM)

重写 `fputc()` 函数：

```c
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
```

> 本项目已同时支持 ARM GCC 和 ARMCC，见 `debug_utils.c` 中的条件编译。

---

## 10. 实战示例

### 10.1 PID 参数在线调节

```c
void uart_debug_data_handler(uint8_t *data, uint16_t len)
{
    // 命令格式: "Kp=1.50 Ki=0.10 Kd=0.05"
    float kp, ki, kd;
    if (sscanf((char*)data, "Kp=%f Ki=%f Kd=%f", &kp, &ki, &kd) == 3) {
        pid_set_params(kp, ki, kd);
        debug_printf("PID updated: Kp=%.3f Ki=%.3f Kd=%.3f\r\n", kp, ki, kd);
    }
}
```

PC 端发送 `Kp=2.00 Ki=0.50 Kd=0.01` 即可实时调整 PID 参数。

### 10.2 传感器数据波形输出

```c
// 主循环中每 10ms 输出一次，PC 端可用 Serial Studio 绘图
void app_task_control_loop(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_tick >= 10) {  // 100Hz 输出
        printf("CH1:%.2f,CH2:%.2f,CH3:%.2f\r\n",
               mpu_get_gyro_x(), mpu_get_gyro_y(), mpu_get_gyro_z());
        last_tick = now;
    }
}
```

### 10.3 调试断言保护

```c
void motor_set_speed(int16_t speed)
{
    DEBUG_ASSERT(speed >= -1000 && speed <= 1000, "Motor speed out of range");
    // 如果 speed 越界，输出断言信息并停机
    // ...
}
```

### 10.4 通信协议调试

```c
// 调试 I2C 传感器通信
uint8_t reg_val;
if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, 0x3B, 1, &reg_val, 1, 100) == HAL_OK) {
    debug_hex_dump("MPU_REG", &reg_val, 1);
} else {
    DEBUG_ERROR("I2C read failed!\r\n");
}
```

---

## 11. 常见问题排查

### Q1: 串口完全没有输出？

```text
检查清单：
□ 硬件连接：PA9(TX) → USB-TTL RX, PA10(RX) → USB-TTL TX, GND → GND
□ 波特率是否匹配（115200）
□ 初始化顺序：MX_DMA_Init() → MX_USART1_UART_Init() → uart_debug_init()
□ DMA 中断是否使能：DMA2_Stream2_IRQn / DMA2_Stream7_IRQn
□ USART1 中断是否使能：USART1_IRQn
□ USB-TTL 转换器驱动是否安装
```

### Q2: printf 输出乱码？

- **波特率不匹配** — 检查 PC 终端波特率是否为 115200
- **数据位/停止位不一致** — 检查 8N1 配置
- **编译器问题** — ARM GCC 需链接 `nosys.specs`（已配置），ARMCC 需重写 `fputc`

### Q3: 数据输出一段时间后停止？

- **DMA TX 忙** — `debug_printf` 调用过于频繁，DMA 来不及发送
- **解决方案**：降低输出频率，或增大发送缓冲区

### Q4: 接收数据不完整或丢失？

- **RX 缓冲区太小** → 增大 `UART_DEBUG_RX_BUF_SIZE`
- **IDLE 回调处理时间过长** → 回调中只做数据拷贝，处理移到主循环
- **未及时重启 DMA 接收** → 确保回调末尾调用 `HAL_UARTEx_ReceiveToIdle_DMA()`
- **CubeMX 重新生成覆盖代码** → 自定义代码需在 `USER CODE BEGIN/END` 区域内

### Q5: 如何与 CubeMX 兼容？

所有自定义代码都位于 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 标记之间。CubeMX 重新生成时不会覆盖这些区域。DMA+IDLE 功能代码（`uart_debug_init()` 等）保留在 `USER CODE` 区域中，不受 CubeMX 重新生成影响。

---

## 12. 相关文件索引

| 文件 | 说明 |
|------|------|
| [`Core/Inc/usart.h`](../../../Core/Inc/usart.h) | USART 初始化及 DMA+IDLE 功能声明 |
| [`Core/Src/usart.c`](../../../Core/Src/usart.c) | USART 初始化及 DMA 收发实现 |
| [`Core/Inc/callback.h`](../../../Core/Inc/callback.h) | HAL 回调函数声明 |
| [`Core/Src/callback.c`](../../../Core/Src/callback.c) | HAL 回调函数实现（核心接收逻辑） |
| [`Core/Inc/debug_utils.h`](../../../Core/Inc/debug_utils.h) | 调试输出工具函数及宏定义 |
| [`Core/Src/debug_utils.c`](../../../Core/Src/debug_utils.c) | printf 重定向及格式化输出实现 |
| [`Core/Src/main.c`](../../../Core/Src/main.c) | 程序入口及初始化示例 |
| [`Core/Src/stm32f4xx_it.c`](../../../Core/Src/stm32f4xx_it.c) | 中断服务函数（USART1 / DMA 中断） |
| [`Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h`](../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h) | HAL UART 驱动头文件 |

---

> **相关教程**：通用串口通信原理及 DMA+IDLE 详解见同目录下的 [`README.md`](./README.md)。
