#include "DebugPrintfTest.h"
#include "DebugPrintf.h"
#include "Callback.h"
#include "dma.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern DMA_HandleTypeDef hdma_usart1_rx;

/* ==================== 全局实例 ==================== */

/**
 * @brief 调试串口 DMA 接收缓冲区
 * @note  必须为全局数组，DMA 在后台持续访问
 */
static uint8_t dbg_rx_buffer[512];

/**
 * @brief DebugPrintf 实例
 */
DebugPrintf_t dbg_printf;

/* ==================== 演示状态机 ==================== */

static uint32_t last_print_tick = 0;
static uint32_t print_counter   = 0;

/* ==================== 十六进制 dump 演示数据 ==================== */

// static const uint8_t demo_data[] = {
//     0xFF, 0x0A, 0x00, 0x3C, 0x1A, 0x2B, 0x00, 0x00,
//     0x00, 0x00, 0x00, 0x00, 0x00, 0x00
// };

/* ==================== 状态机实现 ==================== */

static void Dbg_Demo_Update(DebugPrintf_t *dbg)
{
    // uint32_t now = HAL_GetTick();

    // /*
    //  * =============================================
    //  *  阶段1：周期性打印（每 2 秒一次）
    //  * =============================================
    //  * 演示三种输出方式：
    //  *  - DebugPrintf_Print   → 带 [s.ms] 时间戳
    //  *  - printf              → _write() 重定向到 USART1
    //  *  - DEBUG_INFO 宏       → [INFO] 级别标签
    //  */
    // if (now - last_print_tick >= 2000) {
    //     last_print_tick = now;
    //     print_counter++;

    //     /* 方式1: DebugPrintf_Print — 带时间戳 */
    //     DebugPrintf_Print(dbg,
    //         "Tick=%lu SR=0x%08lX CR1=0x%08lX\r\n",
    //         (unsigned long)now,
    //         (unsigned long)USART1->SR,
    //         (unsigned long)USART1->CR1);

    //     /* 方式2: printf — 通过 _write() 重定向 */
    //     printf("  [printf] counter=%lu, DMA_C=%lu\r\n",
    //            (unsigned long)print_counter,
    //            (unsigned long)__HAL_DMA_GET_COUNTER(&hdma_usart1_rx));

    //     /* 方式3: 调试级别宏 */
    //     DEBUG_INFO("Periodic report #%lu\r\n", (unsigned long)print_counter);

    //     /* 方式4: HexDump — 每 2 次报告输出一次 */
    //     if (print_counter % 2 == 0) {
    //         DebugPrintf_HexDump(dbg, "DEMO", demo_data, sizeof(demo_data));
    //     }

    //     /* 方式5: 状态查询 — 检查 TX 空闲 */
    //     if (!UartBase_IsTxIdle(&dbg->uart)) {
    //         DEBUG_WARN("TX still busy at report time\r\n");
    //     }
    // }

    /*
     * =============================================
     *  阶段2：接收回显（IDLE 中断触发）
     * =============================================
     * 当 DMA+IDLE 收到一帧数据后，rx_done 被置1。
     * 主循环检测到标志后，回显接收到的数据。
     */
    if (dbg->uart.rx_done) {
        uint16_t len = dbg->uart.rx_len;
        dbg->uart.rx_done = 0;

        if (len > 0 && len <= dbg->uart.rx_buf_size) {
            DebugPrintf_Print(dbg, "RX[%u]: ", (unsigned int)len);
            /* 逐字节回显（最多 128 字节，避免发送缓冲区溢出） */
            uint16_t echo_len = (len > 128) ? 128 : len;
            for (uint16_t i = 0; i < echo_len; i++) {
                DebugPrintf_Print(dbg, "%02X ", dbg->uart.rx_buffer[i]);
            }
            DebugPrintf_Print(dbg, "\r\n");
        }
    }
}

/* ==================== 三分法测试函数 ==================== */

/**
  * @brief  DebugPrintf_Test_Init — 第1部分：初始化
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 2 区域
  *         调用时机：外设初始化完成后，while(1) 主循环之前
  *         调用频率：仅一次
  *
  * @note   初始化流程：
  *         1. DebugPrintf_Constructor — 构造子类对象，注册 VTable
  *         2. DebugPrintf_Init — 启动 DMA+IDLE 接收
  *         3. 打印启动信息（演示多种输出方式）
  */
void DebugPrintf_Test_Init(void)
{
    DebugPrintf_Constructor(&dbg_printf, &huart1,
                            dbg_rx_buffer, sizeof(dbg_rx_buffer));
    if (DebugPrintf_Init(&dbg_printf) != 0) {
        Error_Handler();
    }

    last_print_tick = HAL_GetTick();
    print_counter   = 0;

    /* 启动信息 — 演示多种输出方式 */
    DebugPrintf_Print(&dbg_printf,
        "=== UART Test === Buf=%u Clk=%lu ===\r\n",
        (unsigned int)sizeof(dbg_rx_buffer),
        (unsigned long)HAL_RCC_GetSysClockFreq());
}

/**
  * @brief  DebugPrintf_Test_Loop — 第2部分：主循环
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 3 区域
  *         调用时机：while(1) 主循环中每个周期
  *         调用频率：周期性
  *
  * @note   包含任务：
  *         1. 周期性打印（每 2 秒）— DebugPrintf_Print / printf / DEBUG_INFO / HexDump
  *         2. 接收数据回显（IDLE 触发）
  */
void DebugPrintf_Test_Loop(void)
{
    Dbg_Demo_Update(&dbg_printf);
}

/**
  * @brief  DebugPrintf_Test_IRQHandler — 第3部分：中断回调
  *
  * @note   运行位置：Callback.c 的 HAL_UARTEx_RxEventCallback 中
  *
  * @note   RX 接收通过 HAL_UARTEx_RxEventCallback 自动触发
  *         UartBase_RxIdleCallback，无需在此额外处理。
  *         TX 完成和 UART 错误也由 Callback.c 分发。
  *         此函数预留用于其他中断事件。
  */
void DebugPrintf_Test_IRQHandler(void)
{
    /* UART RX/TX/Error 由 Callback.c 中 HAL 回调自动处理 */
}
