#include "ModuleBase.h"     /* 模块基类：Init / Run / Cleanup 等公有接口 */
#include "Buzzer.h"          /* Buzzer 子类：On/Off/Beep/BeepDouble/BeepPattern/RampVolume */
#include "main.h"            /* HAL 硬件定义：BUZZER1_Pin, BUZZER1_GPIO_Port */

/**
  * @brief 蜂鸣器实例（板载蜂鸣器：PC2，高电平响）
  * @note  定义为静态全局变量，便于在调试器中查看状态。
  *         GPIO 已在 MX_GPIO_Init() 中配置为推挽输出。
  */
static Buzzer_t buzzer1;

/* ==================== 演示状态机枚举 ==================== */

typedef enum {
    BUZZER_DEMO_BEEP,           /* 阶段1：单次短鸣 — 演示 Buzzer_Beep */
    BUZZER_DEMO_BEEP_DOUBLE,    /* 阶段2：双连鸣 — 演示 Buzzer_BeepDouble */
    BUZZER_DEMO_BEEP_PATTERN,   /* 阶段3：脉冲序列 — 演示 Buzzer_BeepPattern */
    BUZZER_DEMO_RAMP_UP,        /* 阶段4：音量渐升 — 演示 Buzzer_RampVolume */
    BUZZER_DEMO_RAMP_DOWN       /* 阶段5：音量渐降 — 演示 Buzzer_RampVolume */
} Buzzer_DemoPhase_t;

static Buzzer_DemoPhase_t demo_phase = BUZZER_DEMO_BEEP;
static uint32_t phase_start_tick = 0;
static uint8_t  phase_triggered = 0;  /* 当前阶段的一次性动作是否已触发 */

/* ==================== 中断回调相关状态 ==================== */

static volatile uint32_t irq_call_count = 0;  /* ISR 调用计数器 */
#define BUZZER_IRQ_BEEP_THRESHOLD  1000U       /* 每 1000 次中断触发一次提示音 */

/* ==================== 前向声明 ==================== */

static void Buzzer_Demo_Update(Buzzer_t *bz);

/* ==================== 演示状态机实现 ==================== */

/**
  * @brief  蜂鸣器功能演示状态机
  * @note   依次展示 Buzzer 模块的五大核心功能：
  *         1. Buzzer_Beep()          — 单次短鸣
  *         2. Buzzer_BeepDouble()    — 双连鸣（提示音效果）
  *         3. Buzzer_BeepPattern()   — 自定义脉冲序列
  *         4. Buzzer_RampVolume()    — 音量渐升（非阻塞渐变）
  *         5. Buzzer_RampVolume()    — 音量渐降（非阻塞渐变）
  *
  *         前三个阶段（Beep 系列）为阻塞式鸣叫，在阶段开始时触发一次。
  *         后两个阶段（RampVolume）为非阻塞渐变，由 ModuleBase_Run() 驱动。
  *         每个阶段持续 5 秒，循环往复。
  *
  * @param  bz  指向 Buzzer 对象的指针
  */
static void Buzzer_Demo_Update(Buzzer_t *bz)
{
    uint32_t now       = HAL_GetTick();
    uint32_t elapsed   = now - phase_start_tick;
    uint32_t phase_dur = 5000;   /* 每阶段持续 5 秒 */

    /* 阶段超时 -> 切换到下一阶段 */
    if (elapsed >= phase_dur) {
        demo_phase = (Buzzer_DemoPhase_t)(((int)demo_phase + 1) % 5);
        phase_start_tick = now;
        phase_triggered  = 0;
        elapsed = 0;
    }

    /* 一次性触发检查 */
    if (phase_triggered) {
        return;  /* 当前阶段动作已触发，等待阶段超时 */
    }
    phase_triggered = 1;

    switch (demo_phase) {

    /* ========== 阶段1：单次短鸣 ========== */
    case BUZZER_DEMO_BEEP:
        /*
         * 演示 Buzzer_Beep — 阻塞式单次鸣叫 200ms
         */
        Buzzer_Beep(bz, 200);
        break;

    /* ========== 阶段2：双连鸣 ========== */
    case BUZZER_DEMO_BEEP_DOUBLE:
        /*
         * 演示 Buzzer_BeepDouble — 两次短鸣，间隔 50ms
         * 效果类似设备提示音 "哔哔"
         */
        Buzzer_BeepDouble(bz, 100, 50);
        break;

    /* ========== 阶段3：脉冲序列 ========== */
    case BUZZER_DEMO_BEEP_PATTERN:
        /*
         * 演示 Buzzer_BeepPattern — 3 次脉冲（100ms 响 / 100ms 停）
         */
        Buzzer_BeepPattern(bz, 100, 100, 3);
        break;

    /* ========== 阶段4：音量渐升 ========== */
    case BUZZER_DEMO_RAMP_UP:
        /*
         * 演示 Buzzer_RampVolume 递增（非阻塞）
         * 5 秒内音量从 0% 线性增加到 100%
         * 由 ModuleBase_Run() 中的状态机驱动渐变
         */
        Buzzer_On(bz);
        Buzzer_SetVolume(bz, 0);
        Buzzer_RampVolume(bz, 100, 4800);
        break;

    /* ========== 阶段5：音量渐降 ========== */
    case BUZZER_DEMO_RAMP_DOWN:
        /*
         * 演示 Buzzer_RampVolume 递减（非阻塞）
         * 5 秒内音量从 100% 线性减小到 10%（近乎静音）
         */
        Buzzer_On(bz);
        Buzzer_SetVolume(bz, 100);
        Buzzer_RampVolume(bz, 10, 4800);
        break;

    default:
        break;
    }
}

/* ==================== 三分法测试函数 ==================== */

/**
  * @brief  BuzzerTest_Init — 第1部分：初始化
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 2 区域
  *         调用时机：外设初始化完成后，while(1) 主循环之前
  *         调用频率：仅一次
  *
  * @note   接线信息：
  *         - BUZZER1: PC2, GPIO_PIN_2, 高电平响（有源蜂鸣器）
  *         - 该引脚已在 MX_GPIO_Init() 中配置为推挽输出
  *
  * @note   初始化流程（与 ModuleBase 规范一致）：
  *         1. Buzzer_Constructor  — 构造子类对象，注册虚函数表
  *         2. ModuleBase_Init     — 通过 vtable 调用 Buzzer_init，
  *                                  验证引脚合法性并初始静音
  */
void BuzzerTest_Init(void)
{
    /*
     * 构造：PC2, 有源蜂鸣器, 高电平响
     */
    Buzzer_Constructor(&buzzer1, BUZZER1_GPIO_Port, BUZZER1_Pin,
                       BUZZER_TYPE_ACTIVE, 1);

    if (ModuleBase_Init((ModuleBase_t *)&buzzer1) != 0) {
        Error_Handler();   /* 初始化失败（如端口/引脚参数无效）*/
    }

    /* 初始化提示：短鸣一声表示蜂鸣器就绪 */
    Buzzer_Beep(&buzzer1, 100);

    /* 记录演示起始时间 */
    phase_start_tick = HAL_GetTick();
}

/**
  * @brief  BuzzerTest_Loop — 第2部分：主循环
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 3 区域
  *         调用时机：while(1) 主循环中每个周期
  *         调用频率：周期性（循环迭代速率）
  *
  * @note   包含两个周期性任务：
  *         1. ModuleBase_Run()     — 非阻塞音量渐变驱动（必须周期性调用）
  *         2. Buzzer_Demo_Update() — 演示状态机更新（每循环一次）
  */
void BuzzerTest_Loop(void)
{
    /*
     * =============================================
     *  1. 非阻塞音量渐变刷新（必须周期性调用）
     * =============================================
     *
     * ModuleBase_Run() 通过虚函数表调用 Buzzer_run()，
     * 后者基于 HAL_GetTick() 对音量渐变进行线性插值，
     * 逐步将音量从当前值过渡到目标值。
     *
     * 若不调用此函数，Buzzer_RampVolume() 设置的渐变
     * 将不会生效。
     */
    ModuleBase_Run((ModuleBase_t *)&buzzer1);

    /*
     * =============================================
     *  2. 蜂鸣器演示状态机更新（每循环一次）
     * =============================================
     *
     * 依次演示以下功能（每阶段 5 秒，循环）：
     *   - Buzzer_Beep()         : 单次短鸣
     *   - Buzzer_BeepDouble()   : 双连鸣
     *   - Buzzer_BeepPattern()  : 脉冲序列
     *   - Buzzer_RampVolume()   : 音量渐升
     *   - Buzzer_RampVolume()   : 音量渐降
     */
    Buzzer_Demo_Update(&buzzer1);
}

/**
  * @brief  BuzzerTest_IRQHandler — 第3部分：中断回调
  *
  * @note   运行位置：Callback.c 的中断回调函数中
  *         调用时机：由硬件中断触发（如定时器周期中断）
  *         调用频率：取决于中断频率
  *
  * @note   中断安全规则：
  *         - 本函数仅做计数器和标志位操作，不调用阻塞 API
  *         - 达到阈值后仅翻转标志，实际鸣叫由主循环检测并执行
  *         - 保持 ISR 快速返回
  *
  * @note   使用示例（在 Callback.c 中）：
  *         void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  *             if (htim->Instance == TIM2) {
  *                 BuzzerTest_IRQHandler();
  *             }
  *         }
  */
void BuzzerTest_IRQHandler(void)
{
    /*
     * 中断调用计数器：
     * 每进入一次中断，计数器加 1。
     * 当达到阈值 BUZZER_IRQ_BEEP_THRESHOLD 时，触发一次短鸣。
     * 适用于定时器周期中断（如 TIM2 1kHz -> 每 1 秒触发一次提示音）。
     */
    irq_call_count++;

    if (irq_call_count >= BUZZER_IRQ_BEEP_THRESHOLD) {
        irq_call_count = 0;

        /*
         * 在中断中触发短促鸣叫。
         * 注意：Buzzer_Beep() 内部使用 HAL_Delay() 阻塞等待，
         * 仅适用于低频中断场景。高频中断中应改用非阻塞方式，
         * 如设置标志位后在主循环中执行鸣叫。
         */
        Buzzer_Beep(&buzzer1, 50);
    }
}
