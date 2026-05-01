#ifndef __DEBUG_PRINTF_TEST_H__
#define __DEBUG_PRINTF_TEST_H__

#include "DebugPrintf.h"

/* ==================== 三分法测试接口 ==================== */

void DebugPrintf_Test_Init(void);
void DebugPrintf_Test_Loop(void);
void DebugPrintf_Test_IRQHandler(void);

#endif /* __DEBUG_PRINTF_TEST_H__ */
