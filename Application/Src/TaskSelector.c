/**
  ******************************************************************************
  * @file    TaskSelector.c
  * @brief   按键选择任务模块实现
  *
  * @details
  * 管理 4 个 Key 对象 (KEY1-4)，状态机驱动任务选择与运行:
  *
  *   IDLE:    等待按键单击 → 启动对应任务 → 打印提示
  *   RUNNING: 分发到活跃任务的 Loop 函数, 检测完成 → 返回 IDLE
  *
  * Key_Tick 由 TaskSelector_IRQHandler() 在 TIM2 1ms ISR 中调用,
  * Key_Check 由 TaskSelector_Loop() 在主循环中轮询。
  ******************************************************************************
  */

#include "TaskSelector.h"
#include "Key.h"
#include "Buzzer.h"
#include "DebugPrintf.h"
#include "Init.h"
#include "MoveControl.h"

/* Task 头文件 */
#include "Task1_LineTrack.h"
#include "Task2_LineTrack.h"
#include "Task3_LineTrack.h"
#include "Task4_LineTrack.h"

/* ==================== 私有宏定义 ==================== */

/** @brief 按键选择等待打印间隔 (ms) */
#define SELECTOR_PROMPT_INTERVAL_MS  2000U

/** @brief 按键确认蜂鸣时长 (ms) */
#define SELECTOR_BEEP_MS             80U

/* ==================== 状态枚举 ==================== */

typedef enum {
    SELECTOR_STATE_IDLE    = 0,  /**< 等待按键选择任务 */
    SELECTOR_STATE_RUNNING = 1   /**< 任务运行中 */
} SelectorState_t;

/* ==================== 私有变量 ==================== */

static SelectorState_t selector_state = SELECTOR_STATE_IDLE;
static uint8_t         active_task    = 0;  /**< 1-4: Task1-4, 0: 无 */

static Key_t    key1, key2, key3, key4;
static Buzzer_t selector_buzzer;

static uint32_t last_prompt_tick = 0;

/* ==================== 私有函数声明 ==================== */

static void Selector_LaunchTask(uint8_t task_id);

/* ==================== Part 1: 初始化 ==================== */

/**
  * @brief  TaskSelector 初始化 — 构造 4 个 Key 对象 + 蜂鸣器, 进入 IDLE 状态
  *
  * @note   调用位置: main() → USER CODE BEGIN 2, 在 Framework_Init() 之后。
  *         所有 Key 为低电平按下 (active_low=1)。
  */
void TaskSelector_Init(void)
{
    /* 构造 KEY1 (PC1, 低电平按下) */
    Key_Constructor(&key1, KEY1_GPIO_Port, KEY1_Pin, 1);
    ModuleBase_Init((ModuleBase_t *)&key1);

    /* 构造 KEY2 (PC3, 低电平按下) */
    Key_Constructor(&key2, KEY2_GPIO_Port, KEY2_Pin, 1);
    ModuleBase_Init((ModuleBase_t *)&key2);

    /* 构造 KEY3 (PA0, 低电平按下) */
    Key_Constructor(&key3, KEY3_GPIO_Port, KEY3_Pin, 1);
    ModuleBase_Init((ModuleBase_t *)&key3);

    /* 构造 KEY4 (PA1, 低电平按下) */
    Key_Constructor(&key4, KEY4_GPIO_Port, KEY4_Pin, 1);
    ModuleBase_Init((ModuleBase_t *)&key4);

    /* 蜂鸣器 (PC2, 低电平触发) */
    Buzzer_Constructor(&selector_buzzer, BUZZER1_GPIO_Port, BUZZER1_Pin,
                       BUZZER_TYPE_ACTIVE, 0);
    ModuleBase_Init((ModuleBase_t *)&selector_buzzer);

    selector_state = SELECTOR_STATE_IDLE;
    active_task    = 0;
    last_prompt_tick = HAL_GetTick();

    DebugPrintf_Print(&dbg_printf,
        "\r\n=== Task Selector Ready ===\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  KEY1 → Task1 (CCW Square)\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  KEY2 → Task2 (CW Square + 180°U-turn)\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  KEY3 → Task3 (Load Scenario)\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  KEY4 → Task4 (180°U-turn → Track → 90° → 400pwm/500ms)\r\n");
}

/* ==================== Part 2: ISR 驱动 (1ms) ==================== */

/**
  * @brief  按键状态机驱动 — 在 TIM2 1ms ISR 中调用
  * @note   每个 Key_Tick 内部以 20ms 去抖动间隔执行状态机,
  *         无阻塞调用, 可安全放在 ISR 中。
  */
void TaskSelector_IRQHandler(void)
{
    Key_Tick(&key1);
    Key_Tick(&key2);
    Key_Tick(&key3);
    Key_Tick(&key4);
}

/* ==================== Part 3: 主循环 ==================== */

/**
  * @brief  TaskSelector 主循环 — 按键轮询 + 任务分发
  *
  * @note   调用位置: main() → while(1) 循环中
  *
  *         IDLE 状态: 每 2s 打印一次提示, 检测 KEY1-4 单击事件
  *         RUNNING 状态: 分发到活跃任务的 Loop 函数, 检测完成
  */
void TaskSelector_Loop(void)
{
    uint32_t now = HAL_GetTick();

    switch (selector_state) {

    /* ---- IDLE: 等待按键选择任务 ---- */
    case SELECTOR_STATE_IDLE:
        /* 定期打印选择提示 */
        if (now - last_prompt_tick >= SELECTOR_PROMPT_INTERVAL_MS) {
            last_prompt_tick = now;
            DebugPrintf_Print(&dbg_printf,
                "[Selector] Waiting: KEY1/KEY2/KEY3/KEY4\r\n");
        }

        /* 轮询 KEY1 → Task1 */
        if (Key_Check(&key1, KEY_EVENT_SINGLE)) {
            Selector_LaunchTask(1);
        }
        /* 轮询 KEY2 → Task2 */
        else if (Key_Check(&key2, KEY_EVENT_SINGLE)) {
            Selector_LaunchTask(2);
        }
        /* 轮询 KEY3 → Task3 */
        else if (Key_Check(&key3, KEY_EVENT_SINGLE)) {
            Selector_LaunchTask(3);
        }
        /* 轮询 KEY4 → Task4 */
        else if (Key_Check(&key4, KEY_EVENT_SINGLE)) {
            Selector_LaunchTask(4);
        }
        break;

    /* ---- RUNNING: 任务运行中 ---- */
    case SELECTOR_STATE_RUNNING:
        /* 分发到活跃任务的 Loop 函数 (处理蜂鸣器 + 状态打印) */
        switch (active_task) {
        case 1:
            Task1_LineTrack_Loop();
            break;
        case 2:
            Task2_LineTrack_Loop();
            break;
        case 3:
            Task3_LineTrack_Loop();
            break;
        case 4:
            Task4_LineTrack_Loop();
            break;
        default:
            break;
        }

        /* 检测任务是否完成 */
        if (MoveControl_GetLineTrackDone(&move_ctrl)) {
            DebugPrintf_Print(&dbg_printf,
                "=== Task%d Complete! Returning to selection... ===\r\n",
                active_task);
            active_task = 0;
            selector_state = SELECTOR_STATE_IDLE;
            last_prompt_tick = now;
        }
        break;

    default:
        selector_state = SELECTOR_STATE_IDLE;
        break;
    }
}

/* ==================== 私有函数 ==================== */

/**
  * @brief  启动指定任务
  * @param  task_id  任务编号 (1-4)
  */
static void Selector_LaunchTask(uint8_t task_id)
{
    DebugPrintf_Print(&dbg_printf,
        "=== KEY%d Pressed: Starting Task%d ===\r\n",
        task_id, task_id);

    /* 蜂鸣确认 */
    Buzzer_Beep(&selector_buzzer, SELECTOR_BEEP_MS);

    /* 清除所有 Key 的残留事件 (防止上一任务的误触发) */
    Key_ClearFlag(&key1);
    Key_ClearFlag(&key2);
    Key_ClearFlag(&key3);
    Key_ClearFlag(&key4);

    /* 启动对应任务 */
    switch (task_id) {
    case 1:
        Task1_LineTrack_Init();
        break;
    case 2:
        Task2_LineTrack_Init();
        break;
    case 3:
        Task3_LineTrack_Init();
        break;
    case 4:
        Task4_LineTrack_Init();
        break;
    default:
        return;
    }

    active_task    = task_id;
    selector_state = SELECTOR_STATE_RUNNING;
}
