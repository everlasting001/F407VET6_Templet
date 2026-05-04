# 测试文件三分法结构规则

## 适用范围

`Application/Src/TestProgram/` 目录下所有测试文件（`*Test.c`）必须遵循此三分法结构。

## 三个组成部分

| 函数 | 运行位置 | 职责 | 命名约定 | 调用频率 |
|------|---------|------|---------|---------|
| **Part 1 — Init** | `main()` 的 `USER CODE BEGIN 2` 区域（外设初始化后、while 循环前） | 构造对象、模块初始化、一次性配置 | `<Module>Test_Init()` | 仅一次 |
| **Part 2 — Loop** | `main()` 的 `USER CODE BEGIN 3` 区域（while(1) 主循环内） | 周期性刷新（调用 `ModuleBase_Run()`）、演示状态机更新 | `<Module>Test_Loop()` | 每个循环周期 |
| **Part 3 — IRQHandler** | `Callback.c` 的中断回调函数中（如 `HAL_TIM_PeriodElapsedCallback`） | 中断触发的处理逻辑、计数和标志位操作 | `<Module>Test_IRQHandler()` | 由中断频率决定 |

## 头文件规范

头文件位于 `Application/Inc/TestProgram/<Module>Test.h`，声明三个公有函数，使用 `#ifndef __<MODULE>_TEST_H__` 作为 include guard。

## 命名示例

| 模块 | Init | Loop | IRQHandler |
|------|------|------|------------|
| LED | `LED_Test_Init()` | `LED_Test_Loop()` | `LED_Test_IRQHandler()` |
| Buzzer | `BuzzerTest_Init()` | `BuzzerTest_Loop()` | `BuzzerTest_IRQHandler()` |
| KEY | `KEY_Test_Init()` | `KEY_Test_Loop()` | `KEY_Test_IRQHandler()` |

## IRQHandler 约束

- 保持快速返回，避免阻塞 API（如 `HAL_Delay()`）
- 复杂操作仅设置标志位，由 Loop 函数检测并执行
- 仅在低频中断（< 10Hz）中可直接调用短阻塞操作
