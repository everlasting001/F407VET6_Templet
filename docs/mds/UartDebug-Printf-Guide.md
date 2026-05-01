# UartBase & DebugPrintf 使用指南

## 概述

`UartBase` 是 UART 串口通信基类，`DebugPrintf` 是其子类，提供格式化调试输出。

| 特性 | 说明 |
|------|------|
| 接收方式 | DMA + IDLE 空闲中断（支持变长数据帧） |
| 发送方式 | DMA + TC 传输完成中断（非阻塞） |
| 缓冲区 | 全局/静态数组（DMA 要求），无 malloc |
| printf 重定向 | 不重定向（标准 printf 不受影响） |
| scanf | 不支持 |

## 硬件配置（USART1）

| 信号 | 引脚 | 标签 |
|------|------|------|
| TX | PA9 | TX_DEBUG |
| RX | PA10 | RX_DEBUG |

| DMA 通道 | 方向 | 流 |
|-----------|------|-----|
| USART1_RX | 外设→内存 | DMA2_Stream2 |
| USART1_TX | 内存→外设 | DMA2_Stream7 |

配置：115200bps, 8N1, 无流控

## 类层次结构

```
UartBase (基类)
  ├── init/run/cleanup (VTable 多态)
  ├── SendDMA / SendStr
  ├── StartRx / RxIdleCallback
  └── TxCpltCallback

DebugPrintf (子类，继承 UartBase)
  └── Print(fmt, ...)  — 格式化 DMA 输出
```

## API 参考

### UartBase 公有接口

```c
/* 构造与生命周期 */
void UartBase_Constructor(UartBase_t *self, const char *name,
                          UART_HandleTypeDef *huart,
                          uint8_t *rx_buffer, uint16_t rx_buf_size);
int  UartBase_Init(UartBase_t *self);
int  UartBase_Run(UartBase_t *self);
int  UartBase_Cleanup(UartBase_t *self);

/* 数据收发 */
int  UartBase_SendDMA(UartBase_t *self, const uint8_t *data, uint16_t len);
int  UartBase_SendStr(UartBase_t *self, const char *str);
int  UartBase_StartRx(UartBase_t *self);

/* ISR 回调（由 HAL 回调函数调用） */
void UartBase_RxIdleCallback(UartBase_t *self, uint16_t len);
void UartBase_TxCpltCallback(UartBase_t *self);
```

**状态字段**（应用层轮询）：

| 字段 | 类型 | 说明 |
|------|------|------|
| `rx_done` | `volatile uint8_t` | 收到一帧数据时置 1，主循环清零 |
| `rx_len` | `volatile uint16_t` | 接收到的字节数 |
| `rx_buffer` | `uint8_t*` | 接收缓冲区指针 |
| `tx_busy` | `volatile uint8_t` | 1=发送中，0=空闲 |

### DebugPrintf 公有接口

```c
void DebugPrintf_Constructor(DebugPrintf_t *self, UART_HandleTypeDef *huart,
                             uint8_t *rx_buffer, uint16_t rx_buf_size);
int  DebugPrintf_Init(DebugPrintf_t *self);
int  DebugPrintf_Print(DebugPrintf_t *self, const char *fmt, ...);
```

## 集成步骤

### 第1步：定义接收缓冲区（全局/静态）

```c
/* 在 main.c 或应用文件中 */
static uint8_t dbg_rx_buf[512];
```

### 第2步：声明 DebugPrintf 实例

```c
#include "DebugPrintf.h"

DebugPrintf_t dbg_printf;
```

### 第3步：在 main() 中初始化

```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    /* ... 其他外设初始化 ... */

    /* USER CODE BEGIN 2 */
    DebugPrintf_Constructor(&dbg_printf, &huart1,
                            dbg_rx_buf, sizeof(dbg_rx_buf));
    DebugPrintf_Init(&dbg_printf);
    DebugPrintf_Print(&dbg_printf,
                      "System init OK, Clock=%lu Hz\r\n",
                      HAL_RCC_GetSysClockFreq());
    /* USER CODE END 2 */

    while (1) {
        /* USER CODE BEGIN 3 */
        /* 检查接收数据 */
        if (dbg_printf.uart.rx_done) {
            uint16_t len = dbg_printf.uart.rx_len;
            dbg_printf.uart.rx_done = 0;
            /* 处理 rx_buffer[0..len-1] */
        }
        /* USER CODE END 3 */
    }
}
```

### 第4步：添加 HAL 回调（Callback.c）

```c
#include "UartBase.h"
#include "DebugPrintfTest.h"  /* 或直接 extern dbg_printf */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        UartBase_RxIdleCallback(&dbg_printf.uart, Size);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        UartBase_TxCpltCallback(&dbg_printf.uart);
    }
}
```

## DMA+IDLE 接收原理

```
UART RX 线 ──→ DMA 自动搬运到 rx_buffer ──→ IDLE 检测
                                              │
                    ┌─────────────────────────┘
                    ▼
         HAL_UARTEx_RxEventCallback(huart, Size)
                    │
                    ▼
         UartBase_RxIdleCallback()  ← 记录 rx_len, 置 rx_done=1
                    │
                    ▼
         UartBase_StartRx()  ← 重新启动 DMA+IDLE 接收
                    │
                    ▼
         主循环检测 rx_done → 处理数据 → 清 rx_done
```

**关键点**：
- IDLE 中断在 RX 线空闲超过 1 字节时间后触发
- 每次 IDLE 后必须重新调用 `HAL_UARTEx_ReceiveToIdle_DMA()` 以持续接收
- RX DMA 使用 NORMAL 模式（非循环），IDLE 时 HAL 自动停止 DMA

## 缓冲区大小建议

| 场景 | 建议大小 | 说明 |
|------|---------|------|
| 简单命令（AT 指令） | 128 字节 | 单行命令，AT+XXX\r\n |
| PID 参数调节 | 256 字节 | 一帧含多个参数 |
| 通用调试 | 512 字节 | 可容纳多行数据 |
| 大数据接收 | 1024 字节 | 注意 RAM 占用 |

## 注意事项

1. **DMA 缓冲区必须是全局/静态** — 栈上局部变量会在函数返回后失效
2. **格式化缓冲区在栈上** — `DebugPrintf_Print()` 使用 256 字节栈缓冲区，无需 malloc
3. **TX 不排队** — 若前次 DMA 发送未完成，新的 `Print()` 调用将丢弃数据并返回 -1
4. **不重定向 printf** — 标准 `printf()` 仍使用默认 `_write()`（无输出）。调试请用 `DebugPrintf_Print()`
5. **ISR 安全** — `RxIdleCallback` 和 `TxCpltCallback` 仅操作标志位和重启 DMA，保持快速返回
6. **堆栈大小** — 项目配置的栈为 0x2000 (8KB)，DebugPrintf_Print 栈缓冲 256 字节在安全范围内

## 测试程序

测试文件：`Application/Src/TestProgram/DebugPrintfTest.c`

**测试内容**：
- 每秒打印运行计数和系统滴答
- 接收到数据时回显（十六进制格式）

**在 main.c 中启用**：
```c
/* USER CODE BEGIN 2 */
DebugPrintf_Test_Init();

/* USER CODE BEGIN WHILE */
while (1) {
    DebugPrintf_Test_Loop();
}
```

## 扩展：添加新的 UART 实例

若需要为 USART2/USART3 创建调试输出：

```c
/* 定义独立缓冲区 */
static uint8_t uart2_rx_buf[256];
static DebugPrintf_t dbg2;

/* 构造第二实例 */
DebugPrintf_Constructor(&dbg2, &huart2,
                        uart2_rx_buf, sizeof(uart2_rx_buf));
DebugPrintf_Init(&dbg2);

/* 在 HAL_UARTEx_RxEventCallback 中按实例分发 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        UartBase_RxIdleCallback(&dbg_printf.uart, Size);
    } else if (huart->Instance == USART2) {
        UartBase_RxIdleCallback(&dbg2.uart, Size);
    }
}
```
