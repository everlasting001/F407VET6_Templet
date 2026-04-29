#ifndef __DWT_DELAY_H
#define __DWT_DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void DWT_Delay_Init(void);

/**
 * @brief  微秒级延时
 * @param  us : 要延时的微秒数
 */
void Delay_us(uint32_t us);

/**
 * @brief  毫秒级延时
 * @param  ms : 要延时的毫秒数
 */
void Delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
#endif /* __DWT_DELAY_H */