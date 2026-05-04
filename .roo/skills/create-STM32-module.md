# Skill: 创建 STM32 模块（.c/.h 文件对）

## 用途
为 STM32F407 项目创建新的外设驱动或功能模块，遵循项目代码规范。

## 使用方式
在 Roo Code 中通过 `skill("create-STM32-module", "<模块名>")` 调用。

## 模板结构

### 头文件模板 (`Core/Inc/<module>.h`)
```c
#ifndef __<MODULE_UPPER>_H
#define __<MODULE_UPPER>_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void <Module>_Init(void);
void <Module>_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __<MODULE_UPPER>_H */
```

### 源文件模板 (`Core/Src/<module>.c`)
```c
/* Includes ------------------------------------------------------------------*/
#include "<module>.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

void <Module>_Init(void)
{
    // TODO: 初始化代码
}

void <Module>_DeInit(void)
{
    // TODO: 反初始化代码
}

/* Private functions ---------------------------------------------------------*/
```

## 注意事项
- 放置在 `Core/Src/` 和 `Core/Inc/` 目录下（CubeMX 不会覆盖）
- 命名遵循项目规范：下划线分隔小写（如 `motor_control.c`）
- 在 `CMakeLists.txt` 的 `target_sources()` 中添加新文件
