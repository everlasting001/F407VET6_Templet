/**
  ******************************************************************************
  * @file    Task1_LineTrack.c
  * @brief   Task1 — 正方形边框循迹一圈实现 (100cm×100cm)
  *
  * @details
  * 通过 Framework 层 move_ctrl 全局实例的巡线状态机，实现对 100cm×100cm
  * 正方形边框的完整循迹。核心控制逻辑由 MoveControl_LineTrackUpdate() 在
  * TIM2 ISR (2ms/500Hz) 中执行，本文件负责参数配置、状态监控和调试输出。
  *
  * === 循迹流程 ===
  *   1. Init: 配置巡线参数 → SetLineTrack 启动巡线模式
  *   2. ISR:  MoveControl_LineTrackUpdate() 驱动状态机
  *      FOLLOWING → 检测路口 → FORWARD_ADJUST → TURNING(90°) → EDGE_DONE
  *      第4条边后 → 减速循迹 (PWM 670→250 线性递减/750mm) → 第5边沿停车
  *   3. Loop: 监控 edge_count, 5 条边完成后打印完成信息
  *
  * === 关键参数 (可运行时通过 Vofa 调参) ===
  *   base_pwm              = 670    巡线基准 PWM (减速起点)
  *   k_line                = 100    LineTurn→PWM 增益
  *   turn_pwm              = 430    转弯基准 PWM
  *   adjust_distance_mm    = 60     路口微调前进距离 (传感器到轮轴)
  *   intersection_threshold = 2     路口确认连续次数 (2×2ms=4ms)
  *   slow_pwm              = 250    减速终点 PWM (750mm 处)
  *   fake_turn_threshold   = 750    假路口过滤/减速距离 (mm)
  *
  ******************************************************************************
  */

#include "Task1_LineTrack.h"
#include "Init.h"
#include "MoveControl.h"
#include "LineSensor.h"
#include "Gyro.h"
#include "Encoder.h"
#include "DebugPrintf.h"
#include "Buzzer.h"
#include "main.h"

/* ==================== 私有宏定义 ==================== */

/** @brief 正方形边长 (mm) — 用于参考，实际由路口检测驱动 */
#define SQUARE_EDGE_MM          1000.0f

/** @brief 正方形边数 (5 条边 = 前4条正常循迹 + 第5边沿停车) */
#define SQUARE_TARGET_EDGES     5

/** @brief 路口确认连续次数 (2×2ms=4ms 持续检测) */
#define TASK1_INTERSECTION_THRESHOLD  2

/** @brief 路口微调前进距离 (mm) — 传感器安装位置到轮轴中心距离 */
#define TASK1_ADJUST_DISTANCE_MM      60.0f

/** @brief 巡线基准 PWM (0~1500) */
#define TASK1_BASE_PWM               760.0f

/** @brief LineTurn 增益 (LineTurn→PWM 修正量) */
#define TASK1_K_LINE                 100.0f

/** @brief 转弯基准 PWM (0~1500) */
#define TASK1_TURN_PWM               430.0f

/** @brief 假路口过滤距离阈值 (mm) */
#define TASK1_FAKE_TURN_THRESHOLD_MM 750.0f

/** @brief 微调前进 PWM */
#define TASK1_ADJUST_SPEED_PWM       350.0f

/** @brief 减速终点 PWM (第4个弯后线性递减至 250) */
#define TASK1_SLOW_PWM              250.0f

/** @brief 状态打印间隔 (ms) */
#define TASK1_PRINT_INTERVAL_MS      500U

/* ==================== 私有变量 ==================== */

static uint32_t last_print_tick = 0;

/** @brief Task1 蜂鸣器实例 (PC2, 低电平触发) */
static Buzzer_t task1_buzzer;

/** @brief 蜂鸣器鸣叫时长 (ms) */
#define TASK1_BEEP_ON_MS    150U
#define TASK1_BEEP_OFF_MS   80U

/* ==================== Part 1: 初始化 ==================== */

/**
  * @brief  Task1 初始化 — 配置并启动正方形边框循迹
  *
  * @note   调用位置: main() → USER CODE BEGIN 2
  *         前置条件: Framework_Init() 已完成 (所有模块已初始化)
  *
  *         配置顺序:
  *          1. 设置巡线基础参数 (base_pwm / k_line)
  *          2. 设置状态机参数 (target_edges / threshold / turn_pwm / adjust_mm)
  *          3. 设置微调前进 PWM
  *          4. 调用 SetLineTrack 启动巡线模式
  */
void Task1_LineTrack_Init(void)
{
    /* 0. 设置任务ID (区分状态机行为) */
    MoveControl_SetTaskID(&move_ctrl, TASK_ID_1);

    /* 1. 设置巡线基础参数 (Vofa 可运行时调整) */
    MoveControl_SetBasePWM(&move_ctrl, TASK1_BASE_PWM);
    MoveControl_SetKLine(&move_ctrl, TASK1_K_LINE);

    /* 2. 设置巡线状态机参数 */
    MoveControl_SetLineTrackConfig(&move_ctrl,
                                   SQUARE_TARGET_EDGES,
                                   TASK1_INTERSECTION_THRESHOLD,
                                   TASK1_TURN_PWM,
                                   TASK1_ADJUST_DISTANCE_MM);

    /* 3. 设置假路口过滤阈值 */
    MoveControl_SetFakeTurnThreshold(&move_ctrl, TASK1_FAKE_TURN_THRESHOLD_MM);

    /* 4. 设置微调前进速度 */
    move_ctrl.adjust_speed_pwm = TASK1_ADJUST_SPEED_PWM;
    move_ctrl.slow_pwm         = TASK1_SLOW_PWM;

    /* 5. 初始化蜂鸣器 (PC2, 低电平触发响, 默认输出高电平静音) */
    Buzzer_Constructor(&task1_buzzer, BUZZER1_GPIO_Port, BUZZER1_Pin,
                       BUZZER_TYPE_ACTIVE, 0);
    if (ModuleBase_Init((ModuleBase_t *)&task1_buzzer) != 0) {
        Error_Handler();
    }

    /* 6. 启动巡线模式 (进入 LINE_STATE_FOLLOWING, 开始第 1 条边) */
    MoveControl_SetLineTrack(&move_ctrl, &line_sensor);

    /* 7. 打印启动信息 */
    DebugPrintf_Print(&dbg_printf,
        "=== Task1: Square Border Track Start ===\r\n");
    DebugPrintf_Print(&dbg_printf,
        "  Edges: %d, BasePWM: %.0f, KLine: %.0f, TurnPWM: %.0f\r\n",
        SQUARE_TARGET_EDGES,
        (double)TASK1_BASE_PWM,
        (double)TASK1_K_LINE,
        (double)TASK1_TURN_PWM);

    last_print_tick = HAL_GetTick();
}

/* ==================== Part 2: 主循环 ==================== */

/**
  * @brief  Task1 主循环 — 状态监控与调试输出
  *
  * @note   调用位置: main() → USER CODE BEGIN 3, while(1) 循环中
  *
  *         每 500ms 打印:
  *           - 当前巡线子状态 (line_state)
  *           - 已完成边数 (edge_count)
  *           - LineTurn / 通道位图
  *           - 当前左右轮 PWM
  *
  *         当 edge_count >= 5 时打印完成信息。
  */
void Task1_LineTrack_Loop(void)
{
    uint32_t now = HAL_GetTick();

    /* 蜂鸣器事件处理 (标志由 ISR 设置, 主循环中执行阻塞鸣叫) */
    if (move_ctrl.buzzer_beep_flag == 1) {
        move_ctrl.buzzer_beep_flag = 0;
        Buzzer_Beep(&task1_buzzer, TASK1_BEEP_ON_MS);
    } else if (move_ctrl.buzzer_beep_flag == 2) {
        move_ctrl.buzzer_beep_flag = 0;
        Buzzer_BeepDouble(&task1_buzzer, TASK1_BEEP_ON_MS, TASK1_BEEP_OFF_MS);
    }

    /* 速率限制: 每 500ms 打印一次 */
    if (now - last_print_tick < TASK1_PRINT_INTERVAL_MS) {
        return;
    }
    last_print_tick = now;

    /* 检查是否完成 */
    if (MoveControl_GetLineTrackDone(&move_ctrl)) {
        DebugPrintf_Print(&dbg_printf,
            "=== Task1: COMPLETE! %d edges tracked ===\r\n",
            MoveControl_GetEdgeCount(&move_ctrl));
        return;
    }

    /* 打印运行状态 */
    uint8_t edge = MoveControl_GetEdgeCount(&move_ctrl);

    static const char *state_names[] = {
        "FOLLOWING", "INTER_CONFIRM", "FWD_ADJUST", "TURNING", "EDGE_DONE"
    };
    const char *sname = (move_ctrl.line_state < 5)
                        ? state_names[move_ctrl.line_state]
                        : "UNKNOWN";

    DebugPrintf_Print(&dbg_printf,
        "[Task1] Edge:%d/5 State:%s%s LT:%.1f CH:0x%02X "
        "LPWM:%.0f RPWM:%.0f\r\n",
        edge + 1, sname,
        move_ctrl.is_slow_phase ? "(Slow)" : "",
        (double)move_ctrl.line_turn,
        move_ctrl.line_ch_bits,
        (double)move_ctrl.line_left_pwm,
        (double)move_ctrl.line_right_pwm);
}

/* ==================== Part 3: 中断回调 ==================== */

/**
  * @brief  Task1 中断回调 — 预留
  * @note   循迹控制由 MoveControl_LineTrackUpdate() 在 TIM2 ISR (2ms) 中执行。
  *         此函数预留用于:
  *           - 按键紧急停止
  *           - 外部触发暂停/恢复
  */
void Task1_LineTrack_IRQHandler(void)
{
    /* 预留: 按键启停、紧急停止等中断处理 */
}
