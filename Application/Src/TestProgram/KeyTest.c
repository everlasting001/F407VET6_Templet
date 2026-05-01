/**
  ******************************************************************************
  * @file    KeyTest.c
  * @brief   Key 模块功能测试 — 三分法结构 (Init / Loop / IRQHandler)
  *
  * @details
  * 测试覆盖范围：
  *   - 4 个独立 Key 对象（KEY1 ~ KEY4），分别绑定点位 PC4~PC7
  *   - 电平极性：全部低电平按下 (active_low = 1)
  *   - 事件覆盖：DOWN、UP、HOLD、SINGLE、DOUBLE、LONG_PRESS、REPEAT
  *
  * 交互演示（Loop 中）：
  *   KEY1 单击 → LED1 翻转
  *   KEY2 双击 → LED2 翻转
  *   KEY3 长按 → 蜂鸣器短鸣（连发时每 125ms 响一次）
  *   KEY4 按下保持 → 蜂鸣器持续发声
  *
  * === 接线信息 ===
  *
  *   KEY1: PC4, GPIO_PIN_4, 低电平按下 (active_low = 1)
  *   KEY2: PC5, GPIO_PIN_5, 低电平按下 (active_low = 1)
  *   KEY3: PC6, GPIO_PIN_6, 低电平按下 (active_low = 1)
  *   KEY4: PC7, GPIO_PIN_7, 低电平按下 (active_low = 1)
  *
  *   以上引脚需在 MX_GPIO_Init() 中配置为输入模式（带上拉或下拉）。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "ModuleBase.h"
#include "Key.h"
#include "LED.h"
#include "Buzzer.h"
#include "main.h"

/* ==================== Key 对象定义 ==================== */

#define KEY_TEST_COUNT 4

static Key_t    key_objects[KEY_TEST_COUNT];
static LED_t    led_test;
static Buzzer_t buzzer_test;

/* ==================== 三分法测试函数 ==================== */

/**
  * @brief  Key_Test_Init — 第1部分：初始化
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 2 区域
  *         调用时机：外设初始化完成后，while(1) 主循环之前
  *         调用频率：仅一次
  *
  * @note   初始化流程：
  *         1. Key_Constructor — 构造 4 个 Key 子类对象，注册虚函数表
  *         2. ModuleBase_Init  — 通过 vtable 调用 Key_init，验证引脚合法性
  *         3. LED/Buzzer Constructor — 构造交互外设对象
  *         4. ModuleBase_Init  — 初始化交互外设
  */
void Key_Test_Init(void)
{
    /* 构造 4 个 Key 对象（引脚根据实际硬件调整）*/
    Key_Constructor(&key_objects[0], KEY1_GPIO_Port, KEY1_Pin, 1);
    Key_Constructor(&key_objects[1], KEY2_GPIO_Port, KEY2_Pin, 1);
    Key_Constructor(&key_objects[2], KEY3_GPIO_Port, KEY3_Pin, 1);
    Key_Constructor(&key_objects[3], KEY4_GPIO_Port, KEY4_Pin, 1);

    /* 初始化所有 Key 对象 */
    for (uint8_t i = 0; i < KEY_TEST_COUNT; i++) {
        if (ModuleBase_Init((ModuleBase_t *)&key_objects[i]) != 0) {
            Error_Handler();
        }
    }

    /* 构造交互外设：LED (PC0) 和 Buzzer (PC2) */
    LED_Constructor(&led_test, LED1_GPIO_Port, LED1_Pin, 1);
    Buzzer_Constructor(&buzzer_test, BUZZER1_GPIO_Port, BUZZER1_Pin,
                       BUZZER_TYPE_ACTIVE, 0);

    if (ModuleBase_Init((ModuleBase_t *)&led_test) != 0) {
        Error_Handler();
    }
    if (ModuleBase_Init((ModuleBase_t *)&buzzer_test) != 0) {
        Error_Handler();
    }
}

/**
  * @brief  Key_Test_Loop — 第2部分：主循环
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 3 区域
  *         调用时机：while(1) 主循环中每个周期
  *         调用频率：周期性
  *
  * @note   事件分发逻辑：
  *         KEY1:
  *           SINGLE      → LED 翻转
  *           LONG_PRESS  → Buzzer 短鸣 200ms
  *         KEY2:
  *           DOUBLE      → LED 翻转
  *         KEY3:
  *           LONG_PRESS  → Buzzer 短鸣 50ms（配合 REPEAT 形成连响）
  *           REPEAT      → (LONG_PRESS 处理中已包含鸣响)
  *         KEY4:
  *           DOWN        → Buzzer 开启（按下就响）
  *           UP          → Buzzer 关闭（释放就停）
  *
  * @note   同时调用 ModuleBase_Run 维持 LED 软件 PWM 刷新。
  */
void Key_Test_Loop(void)
{
    /* 维持 LED 软件 PWM */
    // ModuleBase_Run((ModuleBase_t *)&led_test);

    /* ==================== KEY1 事件处理 ==================== */

    /* 单击：LED 翻转 */
    if (Key_Check(&key_objects[0], KEY_EVENT_SINGLE)) {
        LED_Toggle(&led_test);
    }

    /* 长按：蜂鸣器短鸣 */
    if (Key_Check(&key_objects[0], KEY_EVENT_LONG_PRESS)) {
        Buzzer_Beep(&buzzer_test, 200);
    }

    /* ==================== KEY2 事件处理 ==================== */

    /* 双击：LED 翻转 */
    if (Key_Check(&key_objects[1], KEY_EVENT_DOUBLE)) {
        LED_Toggle(&led_test);
    }

    /* ==================== KEY3 事件处理 ==================== */

    /*
     * 长按 + 连发：蜂鸣器周期短鸣，形成 "哔...哔...哔..." 效果。
     * LONG_PRESS 首次触发时响 50ms，REPEAT 每次也响 50ms。
     */
    if (Key_Check(&key_objects[2], KEY_EVENT_LONG_PRESS)) {
        Buzzer_Beep(&buzzer_test, 50);
    }
    if (Key_Check(&key_objects[2], KEY_EVENT_REPEAT)) {
        Buzzer_Beep(&buzzer_test, 50);
    }

    /* ==================== KEY4 事件处理 ==================== */

    /* 按下就响，释放就停 */
    if (Key_Check(&key_objects[3], KEY_EVENT_DOWN)) {
        Buzzer_On(&buzzer_test);
    }
    if (Key_Check(&key_objects[3], KEY_EVENT_UP)) {
        Buzzer_Off(&buzzer_test);
    }
}

/**
  * @brief  Key_Test_IRQHandler — 第3部分：中断回调
  *
  * @note   运行位置：Callback.c 的 HAL_TIM_PeriodElapsedCallback 中
  *         调用时机：由 TIM2 1ms 定时中断触发
  *         调用频率：每 1ms 一次
  *
  * @note   中断安全规则：
  *         - Key_Tick() 内部以 20ms 分频执行状态机，不包含阻塞调用
  *         - 本函数仅执行状态机驱动，不直接操作外设
  *         - 保持 ISR 快速返回（< 10us）
  *
  * @note   使用示例（在 Callback.c 中）：
  *         void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  *             if (htim == &htim2) {
  *                 Key_Test_IRQHandler();
  *                 // ... 其他 ISR 处理
  *             }
  *         }
  */
void Key_Test_IRQHandler(void)
{
    /* 为每个 Key 对象驱动状态机（每 1ms 调用，内部 20ms 分频）*/
    for (uint8_t i = 0; i < KEY_TEST_COUNT; i++) {
        Key_Tick(&key_objects[i]);
    }
}
