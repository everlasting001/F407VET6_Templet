#include "dwt_delay.h"
#include "main.h"
/*
如何解决“抢占”问题？
如果你确定会有其他代码（如中断、RTOS任务）同时使用 DWT，建议采取以下策略：
1. 禁止在中断中使用 DWT 延时
原则： DWT 延时函数（DWT_Delay_us）只能在主循环（main loop）中调用，绝对不要在中断服务程序（ISR）中调用。
原因： 中断应该越快越好，忙等待会阻塞其他中断和主程序。
2. 移除“清零”操作
修改代码： 不要在初始化或延时函数中执行 DWT->CYCCNT = 0;。
逻辑： 只记录差值。无论计数器是多少，只要用 现在的时间 - 开始的时间 就能得到间隔。这样即使其他代码在用，也不会因为别人的重置操作而崩溃。
3. 使用原子操作（进阶）
如果你必须在中断中读取时间戳，建议将读取操作封装成一个“原子”函数，临时关闭中断再读取，防止读取过程中被其他中断打断导致数据错乱。
*/
void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  安全的延时函数 (处理溢出)
 * @param  cycles : 要延时的 CPU 周期数
 */
static __INLINE void DWT_Delay_Cycles(uint32_t cycles)
{
    uint32_t start = DWT->CYCCNT;
    // 无符号减法处理溢出：如果 CYCCNT 回绕，差值依然正确
    while ((DWT->CYCCNT - start) < cycles);
}

/**
 * @brief  微秒级延时
 * @param  us : 要延时的微秒数
 */
void Delay_us(uint32_t us)
{
    // 计算每微秒的周期数
    uint32_t cycles_per_us = SystemCoreClock / 1000000;
    DWT_Delay_Cycles(us * cycles_per_us);
}


/**
 * @brief  毫秒级延时
 * @param  ms : 要延时的毫秒数
 */
void Delay_ms(uint32_t ms)
{
    // 计算每毫秒的周期数
    uint32_t cycles_per_ms = SystemCoreClock / 1000;
    DWT_Delay_Cycles(ms * cycles_per_ms);
}