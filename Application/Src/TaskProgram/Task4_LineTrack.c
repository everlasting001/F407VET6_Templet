/**
  ******************************************************************************
  * @file    Task4_LineTrack.c
  * @brief   Task4 — 初始180°顺时针掉头 → 巡线 → 第1边沿90°转 → 400pwm循迹500ms
  *
  * @details
  * 路线流程:
  *   1. 初始180°顺时针掉头 (LINE_STATE_INITIAL_TURN, 陀螺仪Yaw闭环)
  *   2. 直线循迹, 检测第1个边沿 (LINE_STATE_FOLLOWING)
  *   3. 边沿微调前进 (LINE_STATE_FORWARD_ADJUST)
  *   4. 90°转弯 (LINE_STATE_TURNING, 方向由检测侧自动识别)
  *   5. 400pwm 循迹 500ms 后停止 (LINE_STATE_TASK4_POST_FOLLOW)
  *
  * 控制核心由 MoveControl_LineTrackUpdate() 在 TIM2 ISR (2ms/500Hz) 中执行,
  * 本文件负责参数配置和状态监控。
  ******************************************************************************
  */

#include "Task4_LineTrack.h"
#include "Init.h"
#include "MoveControl.h"
#include "LineSensor.h"
#include "Gyro.h"
#include "Encoder.h"
#include "DebugPrintf.h"
#include "Buzzer.h"
#include "main.h"

/* ==================== 私有宏定义 ==================== */

/** @brief 目标边数 (仅检测第1个边沿) */
#define TASK4_TARGET_EDGES          1

/** @brief 路口确认连续次数 (2×2ms=4ms 持续检测) */
#define TASK4_INTERSECTION_THRESHOLD  2

/** @brief 路口微调前进距离 (mm) — 传感器安装位置到轮轴中心距离 */
#define TASK4_ADJUST_DISTANCE_MM      65.0f

/** @brief 巡线基准 PWM (0~1500) */
#define TASK4_BASE_PWM               670.0f

/** @brief LineTurn 增益 (LineTurn→PWM 修正量) */
#define TASK4_K_LINE                 100.0f

/** @brief 转弯基准 PWM (0~1500) */
#define TASK4_TURN_PWM               430.0f

/** @brief 微调前进 PWM */
#define TASK4_ADJUST_SPEED_PWM       350.0f

/** @brief 状态打印间隔 (ms) */
#define TASK4_PRINT_INTERVAL_MS      500U

/* ==================== 私有变量 ==================== */

static uint32_t last_print_tick = 0;

/** @brief Task4 蜂鸣器实例 (PC2, 低电平触发) */
static Buzzer_t task4_buzzer;

/** @brief 蜂鸣器鸣叫时长 (ms) */
#define TASK4_BEEP_ON_MS    150U
#define TASK4_BEEP_OFF_MS   80U

/* ==================== Part 1: 初始化 ==================== */

/**
  * @brief  Task4 初始化 — 配置并启动任务
  *
  * @note   调用位置: main() → USER CODE BEGIN 2
  *         前置条件: Framework_Init() 已完成 (所有模块已初始化)
  *
  *         配置顺序:
  *          1. 设置任务ID (TASK_ID_4 → 初始180°掉头+特殊路口处理)
  *          2. 设置巡线基础参数 (base_pwm / k_line)
  *          3. 设置状态机参数 (target_edges=1 / threshold / turn_pwm / adjust_mm)
  *          4. 设置微调前进 PWM
  *          5. 初始化蜂鸣器
  *          6. 调用 SetLineTrack 启动 (进入 LINE_STATE_INITIAL_TURN)
  */
void Task4_LineTrack_Init(void)
{
    /* 0. 设置任务ID (区分 Task4 状态机行为) */
    MoveControl_SetTaskID(&move_ctrl, TASK_ID_4);

    /* 1. 设置巡线基础参数 */
    MoveControl_SetBasePWM(&move_ctrl, TASK4_BASE_PWM);
    MoveControl_SetKLine(&move_ctrl, TASK4_K_LINE);

    /* 2. 设置巡线状态机参数 (target_edges=1: 仅检测第1个边沿) */
    MoveControl_SetLineTrackConfig(&move_ctrl,
                                   TASK4_TARGET_EDGES,
                                   TASK4_INTERSECTION_THRESHOLD,
                                   TASK4_TURN_PWM,
                                   TASK4_ADJUST_DISTANCE_MM);

    /* 3. 设置微调前进速度 */
    move_ctrl.adjust_speed_pwm = TASK4_ADJUST_SPEED_PWM;

    /* 4. 初始化蜂鸣器 (PC2, 低电平触发响, 默认输出高电平静音) */
    Buzzer_Constructor(&task4_buzzer, BUZZER1_GPIO_Port, BUZZER1_Pin,
                       BUZZER_TYPE_ACTIVE, 0);
    if (ModuleBase_Init((ModuleBase_t *)&task4_buzzer) != 0) {
        Error_Handler();
    }

    /* 5. 启动巡线模式 (Task4 从初始180°CW掉头开始) */
    MoveControl_SetLineTrack(&move_ctrl, &line_sensor);

    /* 6. 打印启动信息 */
    DebugPrintf_Print(&dbg_printf,
        "=== Task4: 180°CW U-turn → Line Track → 1st Edge 90° → 400pwm/500ms ===\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  BasePWM: %.0f, KLine: %.0f, TurnPWM: %.0f, AdjDist: %.0fmm\r\n",
        (double)TASK4_BASE_PWM,
        (double)TASK4_K_LINE,
        (double)TASK4_TURN_PWM,
        (double)TASK4_ADJUST_DISTANCE_MM);

    last_print_tick = HAL_GetTick();

    /* USART2 (K230) 启动信息 */
    DebugPrintf_Print(&k230_printf,
        "=== Task4: K230 UART Ready, waiting for K230 data... ===\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  K230 UART (USART2) registered for Task4\r\n");
}

/* ==================== Part 2: 主循环 ==================== */

/**
  * @brief  Task4 主循环 — 状态监控与调试输出
  *
  * @note   调用位置: main() → USER CODE BEGIN 3, while(1) 循环中
  *
  *         每 500ms 打印:
  *           - 当前巡线子状态 (line_state)
  *           - LineTurn / 通道位图
  *           - 当前左右轮 PWM
  */
void Task4_LineTrack_Loop(void)
{
    uint32_t now = HAL_GetTick();

    /* 蜂鸣器事件处理 (标志由 ISR 设置, 主循环中执行阻塞鸣叫) */
    if (move_ctrl.buzzer_beep_flag == 1) {
        move_ctrl.buzzer_beep_flag = 0;
        Buzzer_Beep(&task4_buzzer, TASK4_BEEP_ON_MS);
    } else if (move_ctrl.buzzer_beep_flag == 2) {
        move_ctrl.buzzer_beep_flag = 0;
        Buzzer_BeepDouble(&task4_buzzer, TASK4_BEEP_ON_MS, TASK4_BEEP_OFF_MS);
    }

    /* ---- K230 数据接收检测 (每周期检查, 不限制速率) ---- */
    if (k230_printf.uart.rx_done) {
        uint16_t len = k230_printf.uart.rx_len;
        k230_printf.uart.rx_done = 0;
        if (len > 0 && len <= k230_printf.uart.rx_buf_size) {
            DebugPrintf_Print(&k230_printf, "K230 RX[%u]: ", (unsigned int)len);
            uint16_t echo_len = (len > 128U) ? 128U : len;
            for (uint16_t i = 0; i < echo_len; i++) {
                DebugPrintf_Print(&k230_printf, "%02X ",
                    k230_printf.uart.rx_buffer[i]);
            }
            DebugPrintf_Print(&k230_printf, "\r\n");
        }
    }

    /* 速率限制: 每 500ms 打印一次 */
    if (now - last_print_tick < TASK4_PRINT_INTERVAL_MS) {
        return;
    }
    last_print_tick = now;

    /* 检查是否完成 */
    if (MoveControl_GetLineTrackDone(&move_ctrl)) {
        DebugPrintf_Print(&dbg_printf,
            "=== Task4: COMPLETE! ===\r\n");
        return;
    }

    /* 打印运行状态 */
    static const char *state_names[] = {
        "FOLLOWING", "INTER_CONFIRM", "FWD_ADJUST", "TURNING", "EDGE_DONE",
        "FINAL_FOLLOW", "FINAL_DETECT", "INIT_TURN", "POST_FOLLOW"
    };
    const char *sname = (move_ctrl.line_state < 9)
                        ? state_names[move_ctrl.line_state]
                        : "UNKNOWN";

    DebugPrintf_Print(&dbg_printf,
        "[Task4] State:%s Ang:%.0f LT:%.1f CH:0x%02X "
        "LPWM:%.0f RPWM:%.0f\r\n",
        sname,
        (double)move_ctrl.turn_angle,
        (double)move_ctrl.line_turn,
        move_ctrl.line_ch_bits,
        (double)move_ctrl.line_left_pwm,
        (double)move_ctrl.line_right_pwm);
}

/* ==================== Part 3: 中断回调 ==================== */

/**
  * @brief  Task4 中断回调 — 预留
  * @note   循迹控制由 MoveControl_LineTrackUpdate() 在 TIM2 ISR (2ms) 中执行。
  *         此函数预留用于:
  *           - 按键紧急停止
  *           - 外部触发暂停/恢复
  */
void Task4_LineTrack_IRQHandler(void)
{
    /* 预留: 按键启停、紧急停止等中断处理 */
}
