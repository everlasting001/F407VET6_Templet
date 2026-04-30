#include "ModuleBase.h"     /* 模块基类：Init / Run / Cleanup 等公有接口 */
#include "LED.h"            /* LED 子类：On / Off / Toggle / SetBrightness */
#include "main.h"           /* HAL 硬件定义：LED1_Pin, LED1_GPIO_Port */

/**
  * @brief LED1 实例（板载 LED：PC0，高电平点亮）
  * @note  定义为全局变量，便于在调试器中查看状态；
  *        也可声明为 main() 内的局部变量。
  */
static LED_t led1;

/* 演示状态机枚举 */
typedef enum {
    LED_DEMO_BLINK,         /* 阶段1：闪烁 - 演示 On/Off/Toggle */
    LED_DEMO_BREATHE_IN,    /* 阶段2：呼吸渐亮 - 演示 SetBrightness 递增 */
    LED_DEMO_BREATHE_OUT,   /* 阶段3：呼吸渐灭 - 演示 SetBrightness 递减 */
    LED_DEMO_STEADY         /* 阶段4：常亮 50% - 演示稳定 PWM */
} LED_DemoPhase_t;

static LED_DemoPhase_t demo_phase = LED_DEMO_BLINK;
static uint32_t phase_start_tick = 0;   /* 当前阶段起始时刻 */

/* ==================== 中断回调相关状态 ==================== */

static volatile uint8_t irq_led_toggle_flag = 0;  /* ISR 触发 LED 翻转标志 */

/* ==================== 前向声明 ==================== */

static void LED_Demo_Update(LED_t *led);

/* ==================== 演示状态机实现 ==================== */

/**
  * @brief  LED 功能演示状态机
  * @note   依次展示 LED 模块的四大核心功能：
  *         1. 闪烁（On/Off/Toggle）
  *         2. 渐亮（SetBrightness 0->100）
  *         3. 渐灭（SetBrightness 100->0）
  *         4. 常亮 50%（稳定 PWM 输出）
  *
  *         每个阶段持续 5 秒，循环往复。
  *
  * @param  led  指向 LED 对象的指针
  */
static void LED_Demo_Update(LED_t *led)
{
    uint32_t now       = HAL_GetTick();
    uint32_t elapsed   = now - phase_start_tick;  /* 当前阶段已运行时间 (ms) */
    uint32_t phase_dur = 5000;                     /* 每阶段持续 5 秒 */

    /* 阶段超时 -> 切换到下一阶段 */
    if (elapsed >= phase_dur) {
        demo_phase = (LED_DemoPhase_t)(((int)demo_phase + 1) % 4);
        phase_start_tick = now;
        elapsed = 0;
    }

    switch (demo_phase) {

    /* ========== 阶段1：闪烁 ========== */
    case LED_DEMO_BLINK: {
        /*
         * 演示 LED_On / LED_Off / LED_Toggle
         * 每 500ms 翻转一次，共 10 次闪烁
         */
        static uint32_t last_toggle = 0;
        if (elapsed - last_toggle >= 500) {
            last_toggle = elapsed;
            LED_Toggle(led);                    /* 调用翻转接口 */
        }
        break;
    }

    /* ========== 阶段2：渐亮 (0->100) ========== */
    case LED_DEMO_BREATHE_IN: {
        /*
         * 演示 LED_SetBrightness 递增
         * 5 秒内亮度从 0 线性增加到 100
         * 软件 PWM 在 Run() 中非阻塞刷新，此处仅设置目标值
         */
        LED_On(led);                            /* 确保 LED 点亮 */
        uint8_t brightness = (uint8_t)(elapsed * 100 / phase_dur);
        LED_SetBrightness(led, brightness);     /* 设置亮度 */
        break;
    }

    /* ========== 阶段3：渐灭 (100->0) ========== */
    case LED_DEMO_BREATHE_OUT: {
        /*
         * 演示 LED_SetBrightness 递减
         * 5 秒内亮度从 100 线性减小到 0
         */
        LED_On(led);
        uint8_t brightness = (uint8_t)(100 - elapsed * 100 / phase_dur);
        LED_SetBrightness(led, brightness);
        break;
    }

    /* ========== 阶段4：常亮 50% ========== */
    case LED_DEMO_STEADY: {
        /*
         * 演示稳定的软件 PWM 输出
         * 亮度固定在 50%，由 Run() 维持 PWM 波形
         */
        LED_On(led);
        LED_SetBrightness(led, 50);             /* 50% 亮度 */
        break;
    }

    default:
        break;
    }
}

/* ==================== 三分法测试函数 ==================== */

/**
  * @brief  LED_Test_Init — 第1部分：初始化
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 2 区域
  *         调用时机：外设初始化完成后，while(1) 主循环之前
  *         调用频率：仅一次
  *
  * @note   接线信息：
  *         - LED1: PC0, GPIO_PIN_0, 高电平点亮
  *         - 该引脚已在 MX_GPIO_Init() 中配置为推挽输出
  *
  * @note   初始化流程（与 ModuleBase 规范一致）：
  *         1. LED_Constructor -- 构造子类对象，注册虚函数表
  *         2. ModuleBase_Init  -- 通过 vtable 调用 LED_init，
  *                                验证引脚合法性并初始熄灭
  */
void LED_Test_Init(void)
{
    LED_Constructor(&led1, LED1_GPIO_Port, LED1_Pin, 1);  /* 构造：PC0, 高电平亮 */
    if (ModuleBase_Init((ModuleBase_t *)&led1) != 0) {
        Error_Handler();   /* 初始化失败（如端口/引脚参数无效）*/
    }

    /* 记录演示起始时间 */
    phase_start_tick = HAL_GetTick();
}

/**
  * @brief  LED_Test_Loop — 第2部分：主循环
  *
  * @note   运行位置：main() 中 USER CODE BEGIN 3 区域
  *         调用时机：while(1) 主循环中每个周期
  *         调用频率：周期性（循环迭代速率）
  *
  * @note   包含三个周期任务：
  *         1. ModuleBase_Run()     — 非阻塞软件 PWM 刷新（必须周期性调用）
  *         2. LED_Demo_Update()    — 演示状态机更新（每循环一次）
  *         3. ISR 标志检测         — 处理中断回调设置的 LED 翻转请求
  */
void LED_Test_Loop(void)
{
    /*
     * =============================================
     *  1. 非阻塞软件 PWM 刷新（必须周期性调用）
     * =============================================
     *
     * ModuleBase_Run() 通过虚函数表调用 LED_run()，
     * 后者基于 HAL_GetTick() 计算 PWM 相位，
     * 实时调整 GPIO 输出电平以维持目标亮度。
     *
     * 若不调用此函数，亮度 < 100% 时将无法维持 PWM 波形。
     * 调用频率建议 >= 200Hz（即周期 <= 5ms），
     * 以保证 10ms PWM 周期有足够的刷新分辨率。
     */
    ModuleBase_Run((ModuleBase_t *)&led1);

    /*
     * =============================================
     *  2. LED 演示状态机更新（每循环一次）
     * =============================================
     *
     * 依次演示以下功能（每阶段 5 秒，循环）：
     *   - LED_Toggle()        : 开关翻转
     *   - LED_SetBrightness() : 亮度 0->100（渐亮）
     *   - LED_SetBrightness() : 亮度 100->0（渐灭）
     *   - LED_SetBrightness() : 固定 50%（稳态 PWM）
     */
    LED_Demo_Update(&led1);

    /*
     * =============================================
     *  3. ISR 标志检测 — 中断驱动的 LED 控制
     * =============================================
     *
     * 当 KEY1 按下时（EXIT 中断中设置 irq_led_toggle_flag），
     * 主循环检测到标志后切换 LED 开关状态。
     * 这种 "ISR 设标志 + 主循环处理" 的模式保证了中断快速返回。
     */
    if (irq_led_toggle_flag) {
        irq_led_toggle_flag = 0;
        LED_Toggle(&led1);
    }
}

/**
  * @brief  LED_Test_IRQHandler — 第3部分：中断回调
  *
  * @note   运行位置：Callback.c 的中断回调函数中
  *         调用时机：由硬件中断触发（如 GPIO EXTI 按键中断）
  *         调用频率：取决于中断频率
  *
  * @note   中断安全规则：
  *         - 本函数仅设置标志位，不调用阻塞 API
  *         - 实际的 LED 操作在 LED_Test_Loop() 中由主循环执行
  *         - 保持 ISR 快速返回
  *
  * @note   使用示例（在 Callback.c 中）：
  *         void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  *             if (GPIO_Pin == KEY1_Pin) {
  *                 LED_Test_IRQHandler();
  *             }
  *         }
  */
void LED_Test_IRQHandler(void)
{
    /*
     * 中断安全：仅设置标志位。
     * 主循环 LED_Test_Loop() 检测到此标志后执行实际的 LED 操作。
     */
    irq_led_toggle_flag = 1;
}
