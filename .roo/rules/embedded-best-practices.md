# STM32 嵌入式开发最佳实践

> 原始文档: `.claude/rules/embedded-best-practices.md`

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
        uart_rx_complete = 1;  // Set flag, return immediately
    }
}

// 在 main 循环中处理
while (1) {
    if (uart_rx_complete) {
        uart_rx_complete = 0;
        process_data();  // 实际处理逻辑
    }
}
```

## DMA 使用规范
- DMA 缓冲区必须为全局或静态变量
- 确保 DMA 和 CPU 不同时访问同一缓冲区
- 使用 `__HAL_DMA_ENABLE/__HAL_DMA_DISABLE` 宏控制 DMA

## 调试建议
- 优先使用断点而非串口打印进行调试
- 使用 `Error_Handler()` 捕获异常状态
- 利用 OpenOCD + GDB 查看寄存器状态
