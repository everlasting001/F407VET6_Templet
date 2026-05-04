# UART1 DMA+IDLE 串口通信参考指南

> 来源分支: `ba950d8` / `a88c17c` (USART1_DebugCOM)
> 目标: 供其他 STM32F407VET6 项目版本复刻此串口方案

---

## 1. 文件清单 (6 个文件)

| 文件 | 角色 | 类型 |
|------|------|------|
| `Core/Src/usart.c` | USART1 硬件初始化 + 用户层收发 API | CubeMX 生成 + 用户代码 |
| `Core/Inc/usart.h` | USART1 句柄声明 + 缓冲区宏 + API 原型 | CubeMX 生成 + 用户代码 |
| `Core/Src/callback.c` | HAL 回调集中实现 (Tx/Rx/Error/Idle) | 用户文件 |
| `Core/Inc/callback.h` | 回调函数声明 + 状态查询接口 | 用户文件 |
| `Core/Src/debug_utils.c` | printf 重定向 + 格式化调试输出 | 用户文件 |
| `Core/Inc/debug_utils.h` | 调试级别宏 + 格式化输出原型 | 用户文件 |

**辅助文件** (CubeMX 生成，不修改):
- `Core/Src/dma.c` — DMA2 时钟 + NVIC 配置
- `Core/Src/gpio.c` — PA9/PA10 引脚配置
- `Core/Src/main.c` — 初始化序列调用

---

## 2. CubeMX 硬件配置

### 2.1 USART1 参数

| 参数 | 值 |
|------|-----|
| 模式 | Asynchronous |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | None |
| 流控 | None |
| 过采样 | 16 |

### 2.2 引脚映射

| 信号 | 引脚 | GPIO 模式 | 复用功能 |
|------|------|-----------|----------|
| USART1_TX | PA9 | `GPIO_MODE_AF_PP` | `GPIO_AF7_USART1` |
| USART1_RX | PA10 | `GPIO_MODE_AF_PP` | `GPIO_AF7_USART1` |

### 2.3 DMA 配置

| 方向 | DMA 外设 | 流 | 通道 | 模式 | 数据宽度 |
|------|----------|-----|------|------|----------|
| RX (外设→内存) | DMA2 | Stream2 | Channel 4 | **Circular** | Byte |
| TX (内存→外设) | DMA2 | Stream7 | Channel 4 | **Circular** | Byte |

> **注意**: CubeMX 中配置为 Circular 模式。但 `HAL_UARTEx_ReceiveToIdle_DMA()` 内部会将 RX DMA 自动切换为 **Normal** 模式，这是 IDLE 中断能正确报告接收长度的必要条件。

### 2.4 NVIC 中断优先级

| 中断 | 抢占优先级 | 子优先级 |
|------|-----------|---------|
| USART1_IRQn | 0 | 0 |
| DMA2_Stream2_IRQn (RX) | 0 | 0 |
| DMA2_Stream7_IRQn (TX) | 0 | 0 |

---

## 3. 架构总览

### 3.1 DMA + IDLE 中断接收数据流

```
┌──────────┐    ┌──────────┐    ┌───────────┐    ┌──────────────────┐
│  PC 串口  │───▶│  USART1  │───▶│ DMA2_St2  │───▶│ rx_buffer[256]   │
│  助手    │    │  RX (PA10)│    │ (后台搬运) │    │ (全局静态数组)    │
└──────────┘    └──────────┘    └───────────┘    └────────┬─────────┘
                                                          │
                  当 RX 线空闲 > 1 帧时间                    │
                  ┌──────────────┐                         │
                  │ IDLE 中断     │◀────────────────────────┘
                  │ (硬件触发)    │
                  └──────┬───────┘
                         │
                         ▼
                  ┌──────────────────────────────┐
                  │ HAL_UARTEx_RxEventCallback() │
                  │ - Size = 已接收字节数         │
                  │ - 调用 uart_debug_data_handler│
                  │ - 重启 DMA + IDLE 接收       │
                  └──────────────────────────────┘
```

### 3.2 DMA 发送数据流

```
┌────────────┐    ┌──────────────────┐    ┌───────────┐    ┌──────────┐
│ 应用代码    │───▶│ uart_debug_send()│───▶│ DMA2_St7  │───▶│ USART1   │
│ printf/    │    │ - HAL_UART_      │    │ (后台搬运) │    │ TX (PA9) │
│ debug_printf│   │   Transmit_DMA() │    └───────────┘    └──────────┘
└────────────┘    └──────────────────┘
                          │
                          ▼ (发送完成)
                  ┌──────────────────────┐
                  │ HAL_UART_TxCpltCallback()
                  │ - uart_tx_complete = 1│
                  └──────────────────────┘
```

### 3.3 文件依赖关系

```
main.c
  ├── usart.h          → MX_USART1_UART_Init(), uart_debug_init()
  ├── callback.h       → HAL 回调 (由 HAL 库自动调用)
  ├── debug_utils.h    → debug_printf(), printf (via _write)
  └── dma.h            → MX_DMA_Init()

callback.c
  ├── usart.h          → uart_get_rx_buffer(), uart_debug_send()
  └── debug_utils.h    → (可选，数据处理使用)

debug_utils.c
  └── usart.h          → uart_debug_send() (printf 重定向)
```

---

## 4. 代码详细说明

### 4.1 usart.h — 缓冲区大小定义

```c
#define UART_DEBUG_RX_BUF_SIZE  256   // DMA 接收缓冲区大小
#define UART_DEBUG_TX_BUF_SIZE  512   // printf 格式化输出缓冲区大小
```

**RX 缓冲区 (256 字节)**: 一次 IDLE 中断接收的最大数据量。超过 256 字节的数据会分多帧接收。

**TX 缓冲区 (512 字节)**: `debug_printf` 格式化输出的栈缓冲区。512 字节可容纳约 10 行调试信息。`snprintf` 确保不会溢出。

### 4.2 usart.c — 用户层 API

| 函数 | 功能 | 阻塞 |
|------|------|------|
| `uart_debug_init()` | 启动 DMA+IDLE 接收 | 否 |
| `uart_debug_send(data, len)` | DMA 发送二进制数据 | 否 |
| `uart_debug_send_str(str)` | DMA 发送字符串 | 否 |
| `uart_get_rx_buffer()` | 获取接收缓冲区指针 | 否 |

**uart_debug_init() 实现要点**:
```c
void uart_debug_init(void)
{
    // 1. 启动 IDLE 中断接收
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_debug_rx_buffer,
                                 UART_DEBUG_RX_BUF_SIZE);
    // 2. 清除可能残留的 IDLE 标志
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
}
```

**uart_debug_send() 实现要点**:
```c
void uart_debug_send(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return;
    if (HAL_UART_Transmit_DMA(&huart1, data, len) != HAL_OK)
    {
        HAL_UART_Transmit_IT(&huart1, data, len);  // 降级：DMA 忙时用中断
    }
}
```

### 4.3 callback.c — HAL 回调实现

这是整个方案的核心。四个回调函数的角色：

| 回调 | 触发条件 | 做什么 |
|------|----------|--------|
| `HAL_UART_RxCpltCallback` | 普通 DMA 接收完成 | 占位 (IDLE 模式下不触发) |
| `HAL_UART_TxCpltCallback` | DMA 发送完成 | 设置 `uart_tx_complete = 1` |
| `HAL_UART_ErrorCallback` | DMA/UART 错误 | 清除错误标志 → 重启 DMA 接收 |
| `HAL_UARTEx_RxEventCallback` | **IDLE 中断** (核心) | 处理接收数据 → 回显/分发 → 重启 DMA |

**核心回调 — HAL_UARTEx_RxEventCallback**:

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        uint8_t *rx_buf = uart_get_rx_buffer();

        if (Size > 0)
        {
            uart_debug_data_handler(rx_buf, Size);  // 调用应用层处理
            memset(rx_buf, 0, Size);                 // 清空缓冲区
        }

        // 重启 DMA+IDLE 接收 — 这行绝对不能漏！
        HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, UART_DEBUG_RX_BUF_SIZE);
    }
}
```

**错误回调 — 自动恢复机制**:

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_FLAG(huart,
            UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        HAL_UART_DMAStop(huart);
        // 重启接收，确保系统能继续工作
        HAL_UARTEx_ReceiveToIdle_DMA(huart, uart_get_rx_buffer(),
                                     UART_DEBUG_RX_BUF_SIZE);
        __HAL_UART_CLEAR_IDLEFLAG(huart);
    }
}
```

**弱函数 — uart_debug_data_handler**:

```c
__attribute__((weak)) void uart_debug_data_handler(uint8_t *data, uint16_t len)
{
    uart_debug_send(data, len);  // 默认回显
}
```

使用 `__attribute__((weak))` 允许应用层在任意文件中重新定义此函数，覆盖默认的回显行为。

### 4.4 callback.c — 状态查询接口

```c
uint8_t uart_is_tx_idle(void);       // 查询 TX 是否空闲
uint8_t uart_is_rx_ready(void);      // 查询是否有新数据
void    uart_clear_rx_ready(void);   // 消费数据后清除标志
uint16_t uart_get_last_rx_size(void); // 获取上次接收的数据长度
```

### 4.5 debug_utils.c — printf 重定向

```c
int _write(int file, char *ptr, int len)
{
    (void)file;
    if (ptr == NULL || len <= 0) return 0;
    uart_debug_send((uint8_t *)ptr, len);
    return len;
}
```

`_write()` 是 Newlib 的系统调用钩子。`printf()` 内部最终调用 `_write(1, buf, len)`，从而将输出重定向到 USART1。

### 4.6 debug_utils.h — 调试级别宏

| 宏 | 级别阈值 | 作用 |
|----|---------|------|
| `DEBUG_ERROR(fmt, ...)` | ≥1 | 错误信息 |
| `DEBUG_WARN(fmt, ...)` | ≥2 | 警告信息 |
| `DEBUG_INFO(fmt, ...)` | ≥3 | 一般信息 |
| `DEBUG_DEBUG(fmt, ...)` | ≥4 | 详细调试 |

通过修改 `DEBUG_LEVEL` 编译时常量，可以控制哪些调试代码被编译进固件：
```c
#define DEBUG_LEVEL DEBUG_LEVEL_INFO  // 发布时改为 DEBUG_LEVEL_NONE
```

### 4.7 debug_utils.c — 格式化输出

```c
void debug_printf(const char *fmt, ...)
{
    char buf[UART_DEBUG_TX_BUF_SIZE];  // 栈上 512 字节格式化缓冲
    uint32_t tick = HAL_GetTick();
    // 格式: [秒.毫秒] 用户消息
    int len = snprintf(buf, sizeof(buf), "[%lu.%03lu] ", tick/1000, tick%1000);
    va_start(args, fmt);
    vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
    va_end(args);
    uart_debug_send((uint8_t *)buf, total_len);
}
```

输出示例:
```
[12.345] System initialized, ready for debugging
[12.456] [INFO] Debug level: 3, RX buffer: 256 bytes
```

---

## 5. main.c 集成

### 5.1 初始化顺序

```c
int main(void)
{
    HAL_Init();                    // 1. HAL 库初始化
    SystemClock_Config();          // 2. 时钟 168MHz
    MX_GPIO_Init();                // 3. GPIO (PA9/PA10)
    MX_DMA_Init();                 // 4. DMA2 时钟 + NVIC
    MX_USART1_UART_Init();         // 5. USART1 外设初始化
    uart_debug_init();             // 6. ★ 启动 DMA+IDLE 接收
    // ... 之后即可使用 printf / debug_printf
}
```

### 5.2 使用示例

```c
// 方式1: printf (通过 _write 重定向)
printf("ADC: %d, Temp: %.1f\r\n", adc_val, temp);

// 方式2: debug_printf (带时间戳)
debug_printf("Motor speed: %d\r\n", speed);

// 方式3: 级别宏 (可通过 DEBUG_LEVEL 编译开关)
DEBUG_INFO("System ready\r\n");
DEBUG_ERROR("Overcurrent! %d mA\r\n", current);

// 方式4: 十六进制 dump
debug_hex_dump("GYRO", raw_data, 14);

// 方式5: 直接发送
uart_debug_send_str("Hello\r\n");
```

---

## 6. 集成清单 (在新版本中复刻)

### Step 1: CubeMX 配置
- [ ] USART1: Asynchronous, 115200, 8N1
- [ ] PA9 = USART1_TX, PA10 = USART1_RX
- [ ] DMA2 Stream2 = USART1_RX (Circular, Byte, Peripheral→Memory)
- [ ] DMA2 Stream7 = USART1_TX (Circular, Byte, Memory→Peripheral)
- [ ] NVIC: 启用 USART1 全局中断 + DMA2 Stream2 + DMA2 Stream7
- [ ] 生成代码

### Step 2: 复制用户文件
- [ ] 复制 `callback.c` / `callback.h` 到 `Core/Src/` / `Core/Inc/`
- [ ] 复制 `debug_utils.c` / `debug_utils.h` 到 `Core/Src/` / `Core/Inc/`

### Step 3: 修改 usart.c
- [ ] 在 `USER CODE 0` 区域添加 `#include <string.h>` 和 `static uint8_t uart_debug_rx_buffer[UART_DEBUG_RX_BUF_SIZE];`
- [ ] 在 `USER CODE 1` 区域添加 `uart_debug_init()`, `uart_debug_send()`, `uart_debug_send_str()`, `uart_get_rx_buffer()` 四个函数

### Step 4: 修改 usart.h
- [ ] 在 `USER CODE Includes` 添加 `#include <stdint.h>` 和 `#include <stdio.h>`
- [ ] 在 `USER CODE Private defines` 添加 `UART_DEBUG_RX_BUF_SIZE` 和 `UART_DEBUG_TX_BUF_SIZE` 宏
- [ ] 在 `USER CODE Prototypes` 添加四个 API 函数声明

### Step 5: 修改 CMakeLists.txt
- [ ] 在 `target_sources()` 中添加 `Core/Src/callback.c` 和 `Core/Src/debug_utils.c`

### Step 6: 修改 main.c
- [ ] 添加 `#include "callback.h"` 和 `#include "debug_utils.h"`
- [ ] 在 `MX_USART1_UART_Init()` 之后添加 `uart_debug_init();`
- [ ] 可选：在初始化完成后添加启动信息 `debug_printf("Ready\r\n");`

### Step 7: 编译并测试
- [ ] `cmake --preset Debug && cmake --build build/Debug`
- [ ] 烧录后用串口助手连接 PA9/PA10, 115200 8N1
- [ ] 应看到启动信息，发送数据应收到回显

---

## 7. 常见陷阱

### 7.1 DMA 缓冲区放在栈上 — 数据损坏

```c
// ❌ 错误: 缓冲区在栈上，函数返回后被回收
void uart_init(void) {
    uint8_t rx_buf[256];
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, 256);
}

// ✅ 正确: 全局/静态缓冲区
static uint8_t uart_debug_rx_buffer[256];
```

### 7.2 IDLE 回调后忘记重启 DMA — 只收一次

```c
// ❌ 错误: 处理完数据后没有重新启动 DMA 接收
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    process_data(rx_buf, Size);
    // 漏了重启！下一次收不到数据！
}

// ✅ 正确: 必须重新调用 HAL_UARTEx_ReceiveToIdle_DMA()
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    process_data(rx_buf, Size);
    HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, UART_DEBUG_RX_BUF_SIZE);
}
```

### 7.3 ISR 中做耗时操作 — 系统卡死

```c
// ❌ 错误: 在回调中调用阻塞函数
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    HAL_Delay(100);        // 死锁！
    HAL_UART_Transmit();   // 可能死锁！
    complex_calculation(); // CPU 占用太长！
}

// ✅ 正确: 设置标志，主循环处理
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uart_rx_ready = 1;                    // 只设置标志
    uart_last_rx_size = Size;
    HAL_UARTEx_ReceiveToIdle_DMA(...);    // 重启 DMA
}
```

### 7.4 HAL 返回值未检查 — 静默失败

```c
// ❌ 错误
HAL_UART_Transmit_DMA(&huart1, data, len);

// ✅ 正确
if (HAL_UART_Transmit_DMA(&huart1, data, len) != HAL_OK)
{
    // 降级或错误处理
}
```

---

## 8. 快速参考

| 项目 | 值 |
|------|-----|
| 外设 | USART1 |
| 引脚 | PA9 (TX), PA10 (RX) |
| 波特率 | 115200 8N1 |
| RX DMA | DMA2 Stream2, Channel 4, Circular→Normal |
| TX DMA | DMA2 Stream7, Channel 4, Circular |
| RX 缓冲 | uart_debug_rx_buffer[256] (全局) |
| TX 缓冲 | buf[512] (debug_printf 栈上) |
| 接收模式 | `HAL_UARTEx_ReceiveToIdle_DMA()` |
| 发送模式 | `HAL_UART_Transmit_DMA()` |
| 核心回调 | `HAL_UARTEx_RxEventCallback()` |
| printf 重定向 | `_write()` → `uart_debug_send()` |
| 调试级别 | `DEBUG_LEVEL` 编译时宏 |
