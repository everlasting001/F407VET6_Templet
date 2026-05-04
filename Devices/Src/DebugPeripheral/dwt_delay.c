/**
  ******************************************************************************
  * @file    dwt_delay.c
  * @brief   DWT 周期计数器工具实现
  *
  * @details
  * 提供：
  *   - DWT_Init()         初始化 DWT 周期计数器
  *   - DWT_GetTick_us()   获取当前微秒时间戳 (非阻塞)
  *   - DWT_ElapsedUs()    计算自某时间戳起经过的微秒数
  *   - Delay_us()         阻塞式微秒延时 (仅用于短延时)
  *
  * 原理：
  *   DWT->CYCCNT 每个 CPU 时钟周期 (1/168MHz ≈ 5.95ns) 递增一次。
  *   通过 SystemCoreClock (168000000) 将周期数转换为微秒。
  *
  *   避免清零 DWT->CYCCNT —— 参考代码警告清零会破坏其他使用者。
  *   只记录差值，利用无符号整数减法自动处理溢出回绕。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "dwt_delay.h"
#include "stm32f4xx_hal.h"

/* 缓存 us/cycle 换算因子，避免每次除法 */
static uint32_t s_cycles_per_us = 0;

/* ==================== 公有接口实现 ==================== */

void DWT_Init(void)
{
    /* 使能 DWT 跟踪 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 清零并启用周期计数器 */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* 缓存换算因子 */
    s_cycles_per_us = SystemCoreClock / 1000000U;

    /* 确保配置生效 */
    __DSB();
}

uint32_t DWT_GetTick_us(void)
{
    if (s_cycles_per_us == 0) {
        return 0;
    }
    return DWT->CYCCNT / s_cycles_per_us;
}

uint32_t DWT_ElapsedUs(uint32_t start_us)
{
    uint32_t now_us = DWT_GetTick_us();
    /* 无符号减法自动处理 32 位溢出回绕 */
    return now_us - start_us;
}

void Delay_us(uint32_t us)
{
    if (s_cycles_per_us == 0) {
        s_cycles_per_us = SystemCoreClock / 1000000U;
    }

    uint32_t start = DWT->CYCCNT;
    uint32_t target_cycles = us * s_cycles_per_us;

    /* 溢出安全：无符号减法自动处理回绕 */
    while ((DWT->CYCCNT - start) < target_cycles) {
        /* 忙等待 */
    }
}
