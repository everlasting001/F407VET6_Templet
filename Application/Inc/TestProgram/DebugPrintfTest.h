#ifndef __DEBUG_PRINTF_TEST_H__
#define __DEBUG_PRINTF_TEST_H__

#include "DebugPrintf.h"

/* 全局 DebugPrintf 实例（供 Callback.c 中 HAL 回调访问） */
extern DebugPrintf_t dbg_printf;

/* ==================== 三分法测试接口 ==================== */

void DebugPrintf_Test_Init(void);
void DebugPrintf_Test_Loop(void);
void DebugPrintf_Test_IRQHandler(void);

#endif /* __DEBUG_PRINTF_TEST_H__ */
