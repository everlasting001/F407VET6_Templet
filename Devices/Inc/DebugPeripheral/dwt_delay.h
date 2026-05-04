/**
  ******************************************************************************
  * @file    dwt_delay.h
  * @brief   DWT 周期计数器工具 — 微秒级非阻塞计时
  *
  * @details
  * 基于 ARM Cortex-M4 内建的 DWT (Data Watchpoint and Trace) 周期计数器，
  * 提供微秒级时间戳获取，用于非阻塞步进电机时序控制。
  *
  * DWT->CYCCNT 在 SystemCoreClock (168MHz) 频率下递增，
  * 每微秒 = 168 个周期，32 位计数器约 25.6 秒溢出一次。
  * 无符号减法自动处理溢出回绕，无需额外处理。
  *
  * 用法：
  *   DWT_Init();                          // 系统启动时调用一次
  *   uint32_t t0 = DWT_GetTick_us();      // 获取当前微秒时间戳
  *   // ... 稍后 ...
  *   uint32_t elapsed = DWT_ElapsedUs(t0); // 计算经过的微秒数
  *
  ******************************************************************************
  */

#ifndef __DWT_DELAY_H__
#define __DWT_DELAY_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  初始化 DWT 周期计数器
  * @note   必须在系统时钟配置完成后调用。
  *         使能 DWT 跟踪和周期计数器。
  */
void DWT_Init(void);

/**
  * @brief  获取当前微秒时间戳
  * @note   非阻塞，直接读取 DWT->CYCCNT 并转换为微秒。
  *         溢出安全：32 位回绕由调用方通过无符号减法处理。
  * @return uint32_t  当前时间戳 (us)
  */
uint32_t DWT_GetTick_us(void);

/**
  * @brief  计算从某个时间戳到现在的微秒数
  * @note   无符号减法自动处理计数器溢出回绕。
  * @param  start_us  起始时间戳 (来自 DWT_GetTick_us())
  * @return uint32_t  经过的微秒数
  */
uint32_t DWT_ElapsedUs(uint32_t start_us);

/**
  * @brief  阻塞式微秒延时
  * @note   仅用于短延时 (< 10ms)，阻塞期间不响应中断。
  *         主循环逻辑优先使用 DWT_GetTick_us() 非阻塞方式。
  * @param  us  延时的微秒数
  */
void Delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* __DWT_DELAY_H__ */
