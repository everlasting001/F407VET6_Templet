# STM32 USART 串口通信教程

## 目录

1. [串口基础概念](#1-串口基础概念)
2. [STM32 USART 外设概述](#2-stm32-usart-外设概述)
3. [DMA + IDLE 空闲中断接收](#3-dma--idle-空闲中断接收)
4. [本项目串口架构详解](#4-本项目串口架构详解)
5. [API 参考](#5-api-参考)
6. [调试实战技巧](#6-调试实战技巧)
7. [常见问题](#7-常见问题)

---

## 1. 串口基础概念

### 1.1 什么是 USART？

USART（Universal Synchronous/Asynchronous Receiver Transmitter，通用同步/异步收发器）是STM32最常用的串行通信外设之一。

### 1.2 关键参数

| 参数 | 说明 | 本项目配置 |
|------|------|-----------|
| **波特率 (Baud Rate)** | 每秒传输的符号数 | **115200** |
| **数据位 (Data Bits)** | 每帧的数据位数 | **8** |
| **停止位 (Stop Bits)** | 帧结束标志位数 | **1** |
| **校验位 (Parity)** | 错误检测 | **无 (NONE)** |
| **流控制** | 硬件流控 (RTS/CTS) | **无** |

### 1.3 接线方式

```
STM32F407                  USB-TTL 转换器
┌──────────┐              ┌──────────┐
│ PA9  TX  ─────────────> │ RX       │
│ PA10 RX  <───────────── │ TX       │
│          │              │          │
│ GND      ────────────── │ GND      │
└──────────┘              └──────────┘
```

> **注意**：STM32F407VET6 的 USART1 使用 PA9 (TX) 和 PA10 (RX)，复用功能为 `AF7`。

---

## 2. STM32 USART 外设概述

### 2.1 STM32F407VE 的 USART 资源

| USART | TX Pin | RX Pin | 时钟源 | 最大波特率 |
|-------|--------|--------|--------|-----------|
| USART1 | PA9 | PA10 | APB2 (84MHz) | 10.5Mbps |
| USART2 | PA2 | PA3 | APB1 (42MHz) | 5.25Mbps |
| USART3 | PB10 | PB11 | APB1 (42MHz) | 5.25Mbps |
| UART4 | PA0 | PA1 | APB1 (42MHz) | 5.25Mbps |
| UART5 | PC12 | PD2 | APB1 (42MHz) | 5.25Mbps |
| USART6 | PC6 | PC7 | APB2 (84MHz) | 10.5Mbps |

### 2.2 三种传输模式对比

| 模式 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **轮询 (Blocking)** | 简单、可靠 | 阻塞CPU、浪费算力 | 初始化、低速率单次发送 |
| **中断 (IT)** | 非阻塞、响应快 | 每字节触发中断，高负载时CPU占用高 | 低速数据、命令交互 |
| **DMA** | 极低CPU占用、高速传输 | 配置复杂、占用DMA控制器 | 大量数据传输、高速通信、实时控制 |

---

## 3. DMA + IDLE 空闲中断接收

### 3.1 什么是 IDLE 空闲中断？

USART 的 **IDLE（空闲线）中断** 在 USART 接收引脚（RX）空闲超过一帧传输时间时触发。

**关键特点**：
- 是硬件中断，无需软件定时轮询
- 触发时机精确：一个完整数据帧接收完成后立即检测空闲
- 非常适合**不定长数据接收**

### 3.2 工作原理

```
时间轴 →
───────────────────────────────────────────────────

RX 引脚:  ┌──┐  ┌──┐  ┌──┐              ┌─────
          │D1│  │D2│  │D3│  ... 空闲线   │下一帧
          └──┘  └──┘  └──┘              └─────
                       ↑                 ↑
                  DMA传输中          IDLE中断触发
                                    HAL_UARTEx_RxEventCallback()
```

**流程说明**：
1. DMA 在后台自动将 RX 引脚上的数据搬运到内存缓冲区
2. 发送方停止发送后，RX 线保持空闲状态
3. USART 硬件检测到空闲线 → 产生 IDLE 中断
4. HAL 库自动处理中断 → 调用 `HAL_UARTEx_RxEventCallback()`
5. 回调中获取数据长度 → 处理数据 → 重新启动 DMA 接收

### 3.3 为什么要用 DMA + IDLE，而不是每字节中断？

**传统每字节中断方式**：
```c
// 每个字节都进中断！115200bps 约每秒 11520 次中断
void USART1_IRQHandler(void) {
    uint8_t byte = USART1->DR;  // 读一个字节
    buffer[index++] = byte;     // 存到缓冲区
    // ... 频繁中断上下文切换
}
```

**DMA + IDLE 方式**：
```c
// DMA 自动搬运所有字节，仅在一帧结束后进一次中断
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    // Size 可能包含几十个字节，只进一次中断
    process_data(uart_get_rx_buffer(), Size);
}
```

**性能对比**（115200bps, 每帧100字节, CPU主频168MHz）：

| 指标 | 每字节中断 | DMA + IDLE | 优势 |
|------|-----------|------------|------|
| 每秒中断次数 | ~11520 | ~115 | DMA 方式减少 **99%** 中断 |
| CPU占用率 | ~30% | ~1% | DMA 释放 CPU 处理控制算法 |
| 缓冲区溢出风险 | 高 | 低 | DMA 硬件自动处理 |
| 最大可接收速率 | 受中断响应限制 | 高达 10.5Mbps | DMA 可达外设极限 |

### 3.4 HAL 库函数调用链

```c
// ┌─────────────────────────────────────────┐
// │ 步骤1: 启动 DMA + IDLE 接收             │
// │ HAL_UARTEx_ReceiveToIdle_DMA()          │
// └──────────────────┬──────────────────────┘
//                    │
//                    ▼
// ┌─────────────────────────────────────────┐
// │ 步骤2: DMA 自动在后台搬运数据            │
// │ 外设 → 内存  (无需CPU参与)              │
// │ RX 引脚空闲 → IDLE 中断                 │
// └──────────────────┬──────────────────────┘
//                    │
//                    ▼
// ┌─────────────────────────────────────────┐
// │ 步骤3: HAL 处理 IDLE 中断               │
// │ HAL_UART_IRQHandler()                   │
// │   → UART_Receive_Idle_DMA() 内部处理    │
// │   → 调用 HAL_UARTEx_RxEventCallback()   │
// └──────────────────┬──────────────────────┘
//                    │
//                    ▼
// ┌─────────────────────────────────────────┐
// │ 步骤4: 用户回调处理数据                  │
// │ callback.c 中的实现：                   │
// │ 1. 读取 Size 获取数据长度               │
// │ 2. 处理数据                             │
// │ 3. 重新启动 DMA 接收                     │
// └─────────────────────────────────────────┘
```

---

## 4. 本项目串口架构详解

### 4.1 文件组织

```
Core/
├── Inc/
│   ├── usart.h           ← USART 初始化 + DMA+IDLE 功能声明
│   ├── callback.h        ← 所有 HAL 回调函数声明
│   └── debug_utils.h     ← 调试输出工具函数声明
└── Src/
    ├── usart.c           ← MX_USART1_UART_Init() + DMA+IDLE 实现
    ├── callback.c        ← 所有 HAL 回调函数实现 ★核心文件
    ├── debug_utils.c     ← printf 重定向 + 格式化输出
    └── main.c            ← 初始化入口
```

### 4.2 数据流图

```
                   发送方向 (TX)
           ┌──────────────────────────┐
           │  printf("...")           │
           │  debug_printf("...")     │
           │  debug_puts("...")       │
           │  uart_debug_send(...)    │
           └───────────┬──────────────┘
                       │
               ┌───────▼───────┐
               │  debug_utils  │ 格式化/缓冲
               └───────┬───────┘
                       │
               ┌───────▼───────┐
               │  usart.c      │ HAL_UART_Transmit_DMA()
               │  (DMA TX)     │
               └───────┬───────┘
                       │
               ┌───────▼───────┐
               │  USART1 TX    │  → PC 串口终端
               │  (PA9)        │
               └───────────────┘

                   接收方向 (RX)
           ┌───────────────┐
           │  USART1 RX     │ ← PC 串口终端
           │  (PA10)        │
               │
               ▼
           ┌───────────────┐
           │  DMA 自动搬运  │ 无需CPU参与
               │
               ▼
           ┌───────────────┐
           │  IDLE 中断     │ 一帧结束触发
               │
               ▼
           ┌───────────────┐
           │  callback.c    │ HAL_UARTEx_RxEventCallback()
           │  → 处理数据    │
           │  → 重启DMA接收 │
               │
               ▼
           ┌───────────────┐
           │  应用层处理    │ uart_debug_data_handler()
           │  (命令解析等)  │
           └───────────────┘
```

### 4.3 初始化顺序

```c
int main(void)
{
    HAL_Init();                    // 1. HAL库初始化
    SystemClock_Config();          // 2. 系统时钟 (168MHz)
    MX_GPIO_Init();                // 3. GPIO初始化
    MX_DMA_Init();                 // 4. DMA初始化
    MX_USART1_UART_Init();         // 5. USART1初始化（含DMA配置）
    /* ───────────────────────────────── */
    uart_debug_init();             // 6. ★ 启动DMA+IDLE接收
    /* ───────────────────────────────── */
    debug_printf("System OK\r\n"); // 7. 现在可以使用printf了！
    system_tick_init(1000);
    app_init();
    while(1) {
        app_main_loop();
    }
}
```

---

## 5. API 参考

### 5.1 初始化

```c
/**
 * @brief 启动 DMA + IDLE 空闲中断接收
 *
 * 必须在 MX_USART1_UART_Init() 之后调用。
 * 调用后，接收到的数据将通过 HAL_UARTEx_RxEventCallback 返回。
 */
void uart_debug_init(void);
```

### 5.2 数据发送

```c
/**
 * @brief 通过 DMA 发送数据（非阻塞）
 * @param data 数据缓冲区
 * @param len  数据长度
 */
void uart_debug_send(uint8_t *data, uint16_t len);

/**
 * @brief 发送字符串（自动计算长度）
 * @param str 以 '\0' 结尾的字符串
 */
void uart_debug_send_str(char *str);
```

### 5.3 调试输出

```c
/**
 * @brief 格式化输出（自动添加时间戳）
 * 输出格式: [秒.毫秒] 消息
 * 例如: [1.234] Gyro: 0.12 -0.45 9.81
 */
void debug_printf(const char *fmt, ...);

/**
 * @brief 直接输出字符串（无格式化，更快）
 */
void debug_puts(const char *str);

/**
 * @brief 十六进制数据打印
 * 调试通信协议、传感器原始数据的利器
 */
void debug_hex_dump(const char *tag, const uint8_t *data, uint16_t len);

/**
 * @brief printf 直接可用（已重定向到 USART1）
 * 包含 <stdio.h> 后直接使用
 */
int printf(const char *fmt, ...);
```

### 5.4 调试级别宏

```c
// 在 debug_utils.h 中定义，可通过条件编译控制输出
#define DEBUG_LEVEL DEBUG_LEVEL_INFO  // 默认级别

DEBUG_ERROR("error msg");    // [ERR] ...
DEBUG_WARN("warning msg");   // [WARN] ...
DEBUG_INFO("info msg");      // [INFO] ...
DEBUG_DEBUG("debug msg");    // [DEBUG] ... (仅 DEBUG_LEVEL >= 4)
```

### 5.5 接收数据处理

```c
/**
 * @brief 获取DMA接收缓冲区指针
 *
 * 在 HAL_UARTEx_RxEventCallback 中调用此函数
 * 获取接收到的数据。
 */
uint8_t* uart_get_rx_buffer(void);

/**
 * @brief 用户数据处理回调（弱定义，可重写）
 *
 * 用户在其他文件中实现此函数以自定义处理逻辑。
 * 默认实现：回显接收到的数据。
 */
void uart_debug_data_handler(uint8_t *data, uint16_t len);
```

---

## 6. 调试实战技巧

### 6.1 PID 参数在线调节

```c
// 在 uart_debug_data_handler 中实现简单的命令解析
void uart_debug_data_handler(uint8_t *data, uint16_t len)
{
    // 命令格式: "Kp=1.50 Ki=0.10 Kd=0.05\r\n"
    if (strncmp((char*)data, "Kp=", 3) == 0) {
        float kp, ki, kd;
        sscanf((char*)data, "Kp=%f Ki=%f Kd=%f", &kp, &ki, &kd);
        pid_set_params(kp, ki, kd);     // 设置PID参数
        debug_printf("PID updated: %.3f %.3f %.3f\r\n", kp, ki, kd);
    }
}
```

### 6.2 陀螺仪数据实时输出

```c
// 在控制循环中输出数据
void app_task_control_loop(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_tick >= 100) {  // 每100ms输出一次
        float gx = mpu_get_gyro_x();
        float gy = mpu_get_gyro_y();
        float gz = mpu_get_gyro_z();

        // 使用 printf 输出（自动通过DMA发送）
        printf("GYRO:%.2f,%.2f,%.2f\r\n", gx, gy, gz);

        last_tick = now;
    }
}
```

### 6.3 串口终端推荐

| 工具 | 平台 | 推荐理由 |
|------|------|---------|
| **PuTTY** | Windows | 轻量、稳定、支持串口 |
| **MobaXterm** | Windows | 功能丰富、支持多标签 |
| **Serial Studio** | 跨平台 | 支持数据可视化、图表显示 |
| **Arduino IDE 串口监视器** | 跨平台 | 简单易用 |
| **CuteCom** | Linux | Linux 下轻量选择 |

### 6.4 波特率与缓冲区匹配建议

```c
// 缓冲区大小建议：
// 115200bps → 约 11.5KB/s → 256B 缓冲区可缓冲约 22ms 的数据
// 921600bps → 约 92KB/s  → 256B 缓冲区可缓冲约 2.7ms 的数据

// 如果接收频繁或大数据量，请增大 UART_DEBUG_RX_BUF_SIZE
#define UART_DEBUG_RX_BUF_SIZE  512  // 921600bps 下可缓冲约 5.5ms
```

---

## 7. 常见问题

### Q1: 串口没有输出？

```
检查清单：
□ 硬件连接：PA9 (TX) → USB-TTL RX
             PA10 (RX) → USB-TTL TX
             GND → GND
□ 波特率匹配：终端和程序都是 115200
□ 初始化顺序：MX_DMA_Init() → MX_USART1_UART_Init() → uart_debug_init()
□ DMA 中断已使能：DMA2_Stream2_IRQn 和 DMA2_Stream7_IRQn
□ USART1 中断已使能：USART1_IRQn
```

### Q2: printf 输出乱码？

- **原因1**：波特率不匹配（检查终端波特率是否为 115200）
- **原因2**：数据位/停止位设置不一致
- **原因3**：`_write()` 重定向未正确链接（检查编译器是否使用 Newlib）

### Q3: 接收数据不完整或丢失？

```
可能原因：
1. RX DMA 缓冲区太小 → 增大 UART_DEBUG_RX_BUF_SIZE
2. IDLE 中断处理时间过长 → 回调中只做数据拷贝，处理放在主循环
3. 未及时重启 DMA 接收 → 确保在回调末尾重新调用 ReceiveToIdle_DMA
4. DMA 模式冲突 → 检查 RX DMA 是否被其他外设占用
```

### Q4: 如何与 CubeMX 重新生成兼容？

本项目所有自定义代码均位于 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 标记之间。CubeMX 重新生成时不会覆盖这些区域。

如果 USART1 配置发生变化（如修改波特率），CubeMX 会自动更新 `MX_USART1_UART_Init()` 中的参数，而我们的 DMA+IDLE 功能代码仍然保留。

### Q5: 发送 DMA 忙怎么办？

```c
// 方案1：等前一次发送完成（阻塞，但通常很快）
while (HAL_UART_GetState(&huart1) & HAL_UART_STATE_BUSY_TX) {
    // 等待 DMA 发送完成
}

// 方案2：强制停止前一次发送（可能丢失最后几个字节）
HAL_UART_DMAStop(&huart1);
uart_debug_send(data, len);
```

---

## 附录 A：STM32 USART 中断向量

| 中断名称 | 说明 | 本项目优先级 |
|---------|------|------------|
| `USART1_IRQn` | USART1 全局中断（含 IDLE） | 0 (最高) |
| `DMA2_Stream2_IRQn` | USART1 RX DMA | 0 (最高) |
| `DMA2_Stream7_IRQn` | USART1 TX DMA | 0 (最高) |

## 附录 B：相关文件链接

- [usart.h](../../../Core/Inc/usart.h) - USART 头文件
- [usart.c](../../../Core/Src/usart.c) - USART 实现
- [callback.h](../../../Core/Inc/callback.h) - 回调声明
- [callback.c](../../../Core/Src/callback.c) - 回调实现 ★
- [debug_utils.h](../../../Core/Inc/debug_utils.h) - 调试工具声明
- [debug_utils.c](../../../Core/Src/debug_utils.c) - 调试工具实现
- [main.c](../../../Core/Src/main.c) - 程序入口
- [stm32f4xx_it.c](../../../Core/Src/stm32f4xx_it.c) - 中断服务函数
- [stm32f4xx_hal_uart.h](../../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h) - HAL UART 驱动头文件
