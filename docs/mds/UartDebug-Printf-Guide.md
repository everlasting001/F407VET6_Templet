# UartBase & DebugPrintf 使用指南

## 概述

`UartBase` 是 UART 串口通信基类，`DebugPrintf` 是其子类，提供格式化调试输出。

| 特性 | 说明 |
|------|------|
| 接收方式 | DMA + IDLE 空闲中断（支持变长数据帧） |
| 发送方式 | DMA + TC 传输完成中断（非阻塞） |
| 缓冲区 | 全局/静态数组（DMA 要求），无 malloc |
| printf 重定向 | 支持（通过 `_write()` 钩子，`printf()` → USART1） |
| 时间戳 | 自动添加 `[秒.毫秒]` 前缀 |
| 调试级别 | 编译时 `DEBUG_LEVEL` 宏控制（NONE/ERROR/WARN/INFO/DEBUG） |
| 错误恢复 | UART 错误（ORE/NE/FE/PE）自动清除并重启 DMA+IDLE |
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
  ├── SendDMA / SendStr / StartRx
  ├── RxIdleCallback / TxCpltCallback / ErrorCallback
  ├── IsTxIdle / IsRxReady / ClearRxReady / GetLastRxSize
  └── DataHandler (弱函数，默认回显)

DebugPrintf (子类，继承 UartBase)
  ├── Print(fmt, ...)  — 带时间戳的格式化 DMA 输出
  ├── HexDump(label, data, len)  — 十六进制 dump
  └── _write()  — printf 重定向钩子
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
void UartBase_ErrorCallback(UartBase_t *self);

/* 状态查询 */
uint8_t  UartBase_IsTxIdle(const UartBase_t *self);
uint8_t  UartBase_IsRxReady(const UartBase_t *self);
void     UartBase_ClearRxReady(UartBase_t *self);
uint16_t UartBase_GetLastRxSize(const UartBase_t *self);

/* 弱函数数据处理器（应用层可覆盖） */
void UartBase_DataHandler(UartBase_t *self, uint8_t *data, uint16_t len);
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
int  DebugPrintf_HexDump(DebugPrintf_t *self, const char *label,
                         const uint8_t *data, uint16_t len);
```

### 调试级别宏

```c
#define DEBUG_LEVEL  DEBUG_LEVEL_INFO  // 编译时设置

DEBUG_ERROR("Overcurrent! %d mA\r\n", current);   // ≥1 时编译
DEBUG_WARN("Battery low: %d mV\r\n", voltage);     // ≥2 时编译
DEBUG_INFO("System ready, Clock=%lu Hz\r\n", clk); // ≥3 时编译
DEBUG_DEBUG("Raw ADC: %d\r\n", val);               // ≥4 时编译
```

| 级别 | 宏 | 值 | 推荐场景 |
|------|-----|-----|---------|
| NONE | `DEBUG_LEVEL_NONE` | 0 | 发布固件 |
| ERROR | `DEBUG_LEVEL_ERROR` | 1 | 仅错误信息 |
| WARN | `DEBUG_LEVEL_WARN` | 2 | 错误 + 警告 |
| INFO | `DEBUG_LEVEL_INFO` | 3 | 常规调试（默认） |
| DEBUG | `DEBUG_LEVEL_DEBUG` | 4 | 详细调试（含原始数据） |

宏展开后使用全局 `dbg_printf` 实例发送，应用层必须定义 `extern DebugPrintf_t dbg_printf;`。

## 使用方式对比

```c
/* 方式1: printf 重定向 — 标准 C 输出 */
printf("ADC: %d, Temp: %.1f\r\n", adc_val, temp);
// 输出: ADC: 1024, Temp: 25.3

/* 方式2: DebugPrintf_Print — 带时间戳 */
DebugPrintf_Print(&dbg, "Motor speed: %d\r\n", speed);
// 输出: [12.345] Motor speed: 500

/* 方式3: 调试级别宏 — 可通过 DEBUG_LEVEL 编译开关 */
DEBUG_INFO("System ready\r\n");
// 输出: [12.456] [INFO] System ready

/* 方式4: 十六进制 dump — 查看二进制数据 */
DebugPrintf_HexDump(&dbg, "GYRO", raw_data, 14);
// 输出:
// [12.789] GYRO (14 bytes):
//   FF 0A 00 3C 1A 2B 00 00 00 00 00 00 00 00  |.....<.+........|

/* 方式5: 直接发送 — 无格式化 */
UartBase_SendStr(&dbg.uart, "Hello\r\n");
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

    /* 三种输出方式均可使用 */
    printf("=== System Boot ===\r\n");
    DebugPrintf_Print(&dbg_printf,
                      "Clock=%lu Hz\r\n",
                      (unsigned long)HAL_RCC_GetSysClockFreq());
    DEBUG_INFO("Initialization complete\r\n");
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

/* 普通 DMA 接收完成（IDLE 模式下不触发，占位） */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
}

/* IDLE 接收事件（变长帧核心回调） */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        UartBase_RxIdleCallback(&dbg_printf.uart, Size);
    }
}

/* DMA 发送完成 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        UartBase_TxCpltCallback(&dbg_printf.uart);
    }
}

/* UART 错误 — 自动恢复机制（必须添加！） */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        UartBase_ErrorCallback(&dbg_printf.uart);
    }
}
```

## 错误恢复机制

UART 通信过程中可能发生以下错误：

| 错误 | 标志 | 含义 |
|------|------|------|
| ORE | Overrun Error | 数据接收过快，DMA 未及时搬运 |
| NE | Noise Error | 线路噪声导致采样错误 |
| FE | Framing Error | 停止位不正确 |
| PE | Parity Error | 校验位不匹配 |

发生错误时，`HAL_UART_ErrorCallback()` 被触发，`UartBase_ErrorCallback()` 执行：

```
1. 清除 UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE
2. HAL_UART_DMAStop() — 停止当前 DMA 传输
3. UartBase_StartRx() — 重新启动 DMA+IDLE 接收
4. __HAL_UART_CLEAR_IDLEFLAG() — 清除残留 IDLE 标志
```

**无需手动干预**，系统自动恢复到可接收状态。

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
         UartBase_DataHandler()  ← 弱函数（默认回显，可覆盖）
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
- `UartBase_DataHandler()` 是弱函数，应用层可在任意 `.c` 文件中重新定义以覆盖默认回显行为

### 覆盖数据处理器示例

```c
/* 在应用层 .c 文件中重新定义，覆盖默认回显 */
void UartBase_DataHandler(UartBase_t *self, uint8_t *data, uint16_t len)
{
    /* 自定义处理：例如解析 AT 指令 */
    if (len >= 4 && memcmp(data, "AT+P", 4) == 0) {
        UartBase_SendStr(self, "+OK\r\n");
    }
    /* 不调用默认回显，数据由应用层完全接管 */
}
```

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
4. **printf 已重定向** — `printf()` 通过 `_write()` 钩子输出到 USART1，注意 TX 不排队限制
5. **ISR 安全** — 所有回调（RxIdle/TxCplt/Error）仅操作标志位和重启 DMA，保持快速返回
6. **堆栈大小** — 项目配置的栈为 0x2000 (8KB)，DebugPrintf_Print 栈缓冲 256 字节在安全范围内
7. **调试级别可控** — 发布时设置 `#define DEBUG_LEVEL DEBUG_LEVEL_NONE`，所有调试宏编译为空

## 测试程序

测试文件：`Application/Src/TestProgram/DebugPrintfTest.c`

**测试内容**：
- DebugPrintf_Print 带时间戳输出
- printf() 重定向验证
- 调试级别宏（DEBUG_INFO / DEBUG_WARN）
- HexDump 十六进制 dump
- 接收数据回显（IDLE 触发）

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

/* 错误回调同样需要分发 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        UartBase_ErrorCallback(&dbg_printf.uart);
    } else if (huart->Instance == USART2) {
        UartBase_ErrorCallback(&dbg2.uart);
    }
}
```
