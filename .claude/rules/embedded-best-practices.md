# STM32 嵌入式开发最佳实践

## 内存管理

### 栈与堆
- **避免动态分配** - 在资源受限的嵌入式系统中，优先使用静态数组和栈分配
- **检查栈溢出** - 嵌入式系统栈通常很小，设计时要保守估计栈深度
- **禁止在中断中调用 malloc/free** - 可能导致死锁或碎片化

### 全局变量
- 使用 `static` 限制作用域，避免命名空间污染
- 在 `main()` 前初始化所有全局状态
- 避免隐式初始化，明确赋予初值

## HAL 库使用模式

### 初始化顺序
1. **时钟配置** - `SystemClock_Config()` 必须首先调用
2. **GPIO 初始化** - `MX_GPIO_Init()`（STM32CubeMX 生成）
3. **外设初始化** - UART、SPI、I2C 等依赖时钟的外设
4. **中断启用** - `HAL_NVIC_EnableIRQ()` 在所有初始化完成后

### HAL 返回值检查
```c
// 正确：检查 HAL_OK
if (HAL_UART_Transmit(&huart1, data, size, HAL_MAX_DELAY) != HAL_OK) {
    Error_Handler();
}

// 错误：忽略返回值
HAL_UART_Transmit(&huart1, data, size, HAL_MAX_DELAY);  // 危险
```

### 回调函数命名
- 格式：`HAL_<Module>_<Event>Callback()`
- 示例：`HAL_UART_RxCpltCallback()`、`HAL_TIM_PeriodElapsedCallback()`
- 位置：始终放在对应的 `_it.c` 或应用代码中，绝不在 HAL 库中修改

## 中断处理

### 中断安全规则
- **中断处理函数要快** - 避免长时间阻塞，使用标志让主程序处理
- **原子操作** - 使用 `volatile` 修饰被中断和主程序共享的变量
- **禁止在 ISR 中阻塞** - 不调用 `HAL_Delay()`、`HAL_UART_Transmit()`（非 DMA 模式）等阻塞函数

### 中断回调示例
```c
// 好的做法：设置标志，快速返回
volatile uint8_t uart_rx_complete = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        uart_rx_complete = 1;
    }
}

// 主程序处理
int main() {
    while (1) {
        if (uart_rx_complete) {
            uart_rx_complete = 0;
            process_uart_data();
        }
    }
}
```

## 外设配置

### GPIO
- 使用 STM32CubeMX 生成配置，勿手写寄存器
- 模式枚举：`GPIO_MODE_INPUT`、`GPIO_MODE_OUTPUT_PP`、`GPIO_MODE_AF_PP` 等
- Pull 配置：`GPIO_NOPULL`、`GPIO_PULLUP`、`GPIO_PULLDOWN`
- 速度等级：`GPIO_SPEED_FREQ_LOW`（功耗优化）到 `GPIO_SPEED_FREQ_HIGH`

### UART/串口通信
- 优先使用 DMA 模式减轻 CPU 负担
- 中断模式只适合低速率或小数据量
- 缓冲区大小预留余量，避免溢出

### SPI/I2C
- 检查总线状态：`HAL_I2C_IsDeviceReady()` 和 `HAL_I2C_Master_Receive()`
- 设置合理超时：`HAL_MAX_DELAY` 用于关键操作，具体时间用于一般通信
- DMA 模式需要确保缓冲区不在栈上（使用全局/静态数组）

## 调试与测试

### 日志输出
```c
// 使用 printf 重定向到 UART（需要 syscalls.c 适配）
#include <stdio.h>

int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

// 在代码中使用
printf("ADC value: %d\r\n", adc_value);
```

### 断点调试
- 使用 OpenOCD + Cortex-Debug 进行硬件调试
- 检查变量变化：`watch` 命令追踪寄存器/内存修改
- 查看外设寄存器：`.gdbinit` 配置寄存器视图

## 性能优化

### 功耗管理
- 使用睡眠模式：`HAL_PWR_EnterSLEEPMode()`、`HAL_PWR_EnterSTOPMode()`
- 禁用未使用外设的时钟：`__HAL_RCC_<MODULE>_CLK_DISABLE()`
- 设置 GPIO 低功耗模式

### 执行速度
- 关键路径使用 inline 函数或宏
- 避免浮点运算（F407 有 FPU 但不是所有操作都优化）
- 循环展开对高频率操作

## 代码结构

### 目录组织
```
Core/
├── Src/
│   ├── main.c              （主程序）
│   ├── stm32f4xx_it.c      （中断回调）
│   ├── stm32f4xx_hal_msp.c （HAL 回调）
│   └── app_*.c             （应用逻辑）
└── Inc/
    └── app_*.h             （应用头文件）
```

### 模块化设计
- 每个外设/功能单独的 `.c`/`.h` 对
- `app_uart.c/h`、`app_adc.c/h` 等
- 在 `main.c` 的 `USER CODE` 部分调用初始化和主循环

## STM32CubeMX 工作流

### 代码生成保护
- 所有用户代码放在 `/* USER CODE BEGIN/END */` 标记内
- 不要在标记外添加代码，再生成时会丢失
- 自定义模块创建在 `Core/Src` 中的新文件，不在生成文件中修改

### 常见问题
- **生成后编译失败** - 检查 `CMakeLists.txt` 是否包含新文件
- **中断不工作** - 确认 NVIC 在 CubeMX 中启用，且优先级合理
- **外设初始化顺序** - 遵循 CubeMX 生成的顺序，勿改动 `main.c` 的初始化序列

## 常见陷阱

| 陷阱 | 症状 | 解决方案 |
|------|------|--------|
| HAL 返回值未检查 | 硬件不工作但无错误信息 | 每个 HAL 调用都检查 `!= HAL_OK` |
| ISR 中调用阻塞函数 | 系统卡死或异常 | ISR 只设置标志，主程序处理 |
| 栈溢出 | 难以追踪的奇异行为 | 减少本地数组大小，用静态代替 |
| DMA 缓冲在栈上 | 数据损坏 | DMA 缓冲必须是全局/静态 |
| 时钟未初始化 | 外设无反应 | `SystemClock_Config()` 必须首先调用 |
| 忘记 `HAL_NVIC_EnableIRQ()` | 中断永不触发 | 在所有初始化后启用 NVIC |

## 资源

- **STM32F407 参考手册** - 寄存器定义和时序
- **STM32 HAL API 文档** - 函数原型和使用示例
- **CubeMX 生成代码注释** - 自动生成的配置解释
