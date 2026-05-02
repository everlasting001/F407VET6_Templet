/**
  ******************************************************************************
  * @file    MoveControl.c
  * @brief   运动控制系统实现 — 级联 PID 双轮差速控制
  *
  * @details
  * 算法参考 Algorithm.md 的 GoStraight_Control 三环级联结构：
  *   1. 读取左右编码器距离/速度，取平均位移
  *   2. 位置环 PID：距离误差 → 基础速度 base_velocity (RPM)
  *   3. 差速修正 PD：dist_diff*Kp + derivative*Kd → turn_correction (RPM)
  *   4. 左轮速度环：target = base_vel + turn_correction → PWM_L
  *   5. 右轮速度环：target = base_vel - turn_correction → PWM_R
  *   6. 输出限幅后写入电机 PWM
  *
  * 重要：左右轮 PID 必须分开计算（独立 PID_t 实例），防止数据覆盖。
  *
  * === 控制周期 ===
  * MoveControl_Update() 应在 40ms 固定周期（25Hz）中调用，与编码器更新同步。
  *
  ******************************************************************************
  */

#include "MoveControl.h"
#include <math.h>

/* 默认 PID 参数 (位置环: 距离→RPM) */
#define DEFAULT_POS_KP            1.00f
#define DEFAULT_POS_KI            0.05f
#define DEFAULT_POS_KD            0.06f
#define DEFAULT_POS_INTEGRAL_LIM  200.0f
#define DEFAULT_POS_OUTPUT_LIM    600.0f   /* RPM */

/* 默认 PID 参数 (速度环: RPM→PWM)
   PWM 范围 0~2099, 电机最大转速约 300 RPM,
   kp=5 确保 100RPM 误差 → 500 PWM, kd 提供阻尼防振荡 */
#define DEFAULT_VEL_KP            6.00f
#define DEFAULT_VEL_KI            0.50f
#define DEFAULT_VEL_KD            0.05f
#define DEFAULT_VEL_INTEGRAL_LIM  400.0f
#define DEFAULT_VEL_OUTPUT_LIM    1500.0f  /* PWM */

/* 默认差速修正参数 (PID 控制: mm→RPM, mm/周期→RPM)
   I 项用于消除左右轮速度系统性差异导致的稳态距离偏差 */
#define DEFAULT_BALANCE_KP        5.0f
#define DEFAULT_BALANCE_KI        0.30f
#define DEFAULT_BALANCE_KD        0.05f
#define DEFAULT_BALANCE_INTEGRAL_LIM  300.0f
#define DEFAULT_PWM_LIMIT         1800.0f

/* 控制周期 (秒) */
#define CONTROL_DT                0.04f

/* 停止判定阈值 */
#define STOP_POS_ERROR_MM         30.0f
#define STOP_BASE_VEL_RPM         3.0f

/* 巡线默认参数 */
#define DEFAULT_BASE_PWM          300.0f
#define DEFAULT_K_LINE            500.0f
#define DEFAULT_LINE_W0           -3.0f
#define DEFAULT_LINE_W1           -1.0f
#define DEFAULT_LINE_W2           -0.2f
#define DEFAULT_LINE_W3           -0.02f
#define DEFAULT_LINE_W4           0.02f
#define DEFAULT_LINE_W5           0.2f
#define DEFAULT_LINE_W6           1.0f
#define DEFAULT_LINE_W7           3.0f

/* ==================== 公有接口实现 ==================== */

void MoveControl_Init(MoveControl_t *ctrl,
                      DCMotor_t *motor_left, DCMotor_t *motor_right,
                      Encoder_t *encoder_left, Encoder_t *encoder_right)
{
    if (ctrl == NULL) return;

    ctrl->motor_left     = motor_left;
    ctrl->motor_right    = motor_right;
    ctrl->encoder_left   = encoder_left;
    ctrl->encoder_right  = encoder_right;

    ctrl->mode           = MOVE_MODE_POSITION;
    ctrl->target_mm      = 0.0f;
    ctrl->target_speed_mmps = 0.0f;

    /* 初始化位置环 PID (输出: RPM) */
    PID_Init(&ctrl->pos_pid, DEFAULT_POS_KP, DEFAULT_POS_KI, DEFAULT_POS_KD,
             DEFAULT_POS_INTEGRAL_LIM, DEFAULT_POS_OUTPUT_LIM);

    /* 初始化左轮速度环 PID (输出: PWM) */
    PID_Init(&ctrl->vel_l_pid, DEFAULT_VEL_KP, DEFAULT_VEL_KI, DEFAULT_VEL_KD,
             DEFAULT_VEL_INTEGRAL_LIM, DEFAULT_VEL_OUTPUT_LIM);

    /* 初始化右轮速度环 PID (输出: PWM) — 必须独立实例 */
    PID_Init(&ctrl->vel_r_pid, DEFAULT_VEL_KP, DEFAULT_VEL_KI, DEFAULT_VEL_KD,
             DEFAULT_VEL_INTEGRAL_LIM, DEFAULT_VEL_OUTPUT_LIM);

    ctrl->balance_kp     = DEFAULT_BALANCE_KP;
    ctrl->balance_ki     = DEFAULT_BALANCE_KI;
    ctrl->balance_kd     = DEFAULT_BALANCE_KD;
    ctrl->balance_integral = 0.0f;
    ctrl->last_dist_diff = 0.0f;
    ctrl->pwm_limit      = DEFAULT_PWM_LIMIT;

    /* 巡线控制默认参数 */
    ctrl->line_sensor     = NULL;
    ctrl->base_pwm        = DEFAULT_BASE_PWM;
    ctrl->k_line          = DEFAULT_K_LINE;
    ctrl->line_weights[0] = DEFAULT_LINE_W0;
    ctrl->line_weights[1] = DEFAULT_LINE_W1;
    ctrl->line_weights[2] = DEFAULT_LINE_W2;
    ctrl->line_weights[3] = DEFAULT_LINE_W3;
    ctrl->line_weights[4] = DEFAULT_LINE_W4;
    ctrl->line_weights[5] = DEFAULT_LINE_W5;
    ctrl->line_weights[6] = DEFAULT_LINE_W6;
    ctrl->line_weights[7] = DEFAULT_LINE_W7;
    ctrl->line_turn       = 0.0f;
    ctrl->line_left_pwm   = 0.0f;
    ctrl->line_right_pwm  = 0.0f;
    ctrl->line_ch_bits    = 0;

    ctrl->state          = MOVE_STATE_IDLE;
    ctrl->start_tick     = 0;
}

void MoveControl_SetTarget(MoveControl_t *ctrl, float target_mm)
{
    if (ctrl == NULL) return;

    /* 清零编码器累积数据 */
    if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
    if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);

    /* 复位 PID 历史状态 */
    PID_Reset(&ctrl->pos_pid);
    PID_Reset(&ctrl->vel_l_pid);
    PID_Reset(&ctrl->vel_r_pid);
    ctrl->balance_integral = 0.0f;
    ctrl->last_dist_diff = 0.0f;

    ctrl->mode           = MOVE_MODE_POSITION;
    ctrl->target_mm      = target_mm;
    ctrl->start_tick     = HAL_GetTick();
    ctrl->state          = MOVE_STATE_RUNNING;
}

void MoveControl_Update(MoveControl_t *ctrl)
{
    if (ctrl == NULL) return;

    /* 巡线模式由 MoveControl_LineTrackUpdate() 在 5ms ISR 中独立执行 */
    if (ctrl->mode == MOVE_MODE_LINE_TRACK) return;

    if (ctrl->motor_left == NULL || ctrl->motor_right == NULL
        || ctrl->encoder_left == NULL || ctrl->encoder_right == NULL) {
        return;
    }

    if (ctrl->state != MOVE_STATE_RUNNING) return;

    /* ---- 1. 反馈获取 (Encoder 内部已通过 polarity 处理符号) ---- */
    float dis_l = Encoder_GetDistance(ctrl->encoder_left);
    float dis_r = Encoder_GetDistance(ctrl->encoder_right);
    float vel_l = Encoder_GetRPM(ctrl->encoder_left);
    float vel_r = Encoder_GetRPM(ctrl->encoder_right);
    float avg_dist = (dis_l + dis_r) * 0.5f;

    /* ---- 2. 位置环 PID：距离误差 → 基础速度 (RPM) ---- */
    float base_vel = PID_Compute(&ctrl->pos_pid,
                                 ctrl->target_mm, avg_dist, CONTROL_DT);

    /* ---- 3. 停止判定 ---- */
    float pos_error = ctrl->target_mm - avg_dist;
    if (fabsf(pos_error) < STOP_POS_ERROR_MM
        && fabsf(base_vel) < STOP_BASE_VEL_RPM) {
        DCMotor_Stop(ctrl->motor_left);
        DCMotor_Stop(ctrl->motor_right);
        ctrl->state = MOVE_STATE_COMPLETE;
        return;
    }

    /* ---- 4. 差速修正 PID：P + I + D → 转向修正量 (RPM) ---- */
    float dist_diff = dis_r - dis_l;
    float derivative = dist_diff - ctrl->last_dist_diff;
    ctrl->last_dist_diff = dist_diff;

    /* I 项: 累积左右轮距离差，消除系统性速度差异导致的稳态偏差 */
    ctrl->balance_integral += dist_diff * CONTROL_DT;
    if (ctrl->balance_integral >  DEFAULT_BALANCE_INTEGRAL_LIM)
        ctrl->balance_integral =  DEFAULT_BALANCE_INTEGRAL_LIM;
    if (ctrl->balance_integral < -DEFAULT_BALANCE_INTEGRAL_LIM)
        ctrl->balance_integral = -DEFAULT_BALANCE_INTEGRAL_LIM;

    float turn_correction = dist_diff       * ctrl->balance_kp
                          + ctrl->balance_integral * ctrl->balance_ki
                          + derivative      * ctrl->balance_kd;

    /* ---- 5. 左轮速度环：base_vel + turn_correction → PWM ---- */
    float vel_l_target = base_vel + turn_correction;
    float pwm_l = PID_Compute(&ctrl->vel_l_pid,
                              vel_l_target, vel_l, CONTROL_DT);

    /* ---- 6. 右轮速度环：base_vel - turn_correction → PWM ---- */
    float vel_r_target = base_vel - turn_correction;
    float pwm_r = PID_Compute(&ctrl->vel_r_pid,
                              vel_r_target, vel_r, CONTROL_DT);

    /* ---- 7. 输出限幅 ---- */
    if (pwm_l >  ctrl->pwm_limit) pwm_l =  ctrl->pwm_limit;
    if (pwm_l < -ctrl->pwm_limit) pwm_l = -ctrl->pwm_limit;
    if (pwm_r >  ctrl->pwm_limit) pwm_r =  ctrl->pwm_limit;
    if (pwm_r < -ctrl->pwm_limit) pwm_r = -ctrl->pwm_limit;

    /* ---- 8. 执行 ---- */
    DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);
    DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);
}

/**
  * @brief  巡线修正更新 (5ms/200Hz, 在 TIM2 ISR 5ms 分频中调用)
  * @note   仅处理 MOVE_MODE_LINE_TRACK 模式，其他模式静默返回。
  *         与 LineSensor 扫描同频 (200Hz)，确保修正延迟最小。
  */
void MoveControl_LineTrackUpdate(MoveControl_t *ctrl)
{
    if (ctrl == NULL) return;
    if (ctrl->mode != MOVE_MODE_LINE_TRACK) return;
    if (ctrl->state != MOVE_STATE_RUNNING) return;
    if (ctrl->line_sensor == NULL || ctrl->motor_left == NULL
        || ctrl->motor_right == NULL) return;

    const uint8_t *ch = LineSensor_GetChannelValues(ctrl->line_sensor);
    if (ch == NULL) return;

    /* 计算 LineTurn = Σ(channel[i] × weight[i]) */
    float line_turn = 0.0f;
    uint8_t ch_bits = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (ch[i]) {
            line_turn += ctrl->line_weights[i];
            ch_bits |= (1 << i);
        }
    }

    /* 存储遥测数据 */
    ctrl->line_turn    = line_turn;
    ctrl->line_ch_bits = ch_bits;

    /* 左右轮 PWM = base_pwm ± LineTurn × k_line */
    float pwm_l = ctrl->base_pwm + line_turn * ctrl->k_line;
    float pwm_r = ctrl->base_pwm - line_turn * ctrl->k_line;

    /* 输出限幅 */
    if (pwm_l >  ctrl->pwm_limit) pwm_l =  ctrl->pwm_limit;
    if (pwm_l < -ctrl->pwm_limit) pwm_l = -ctrl->pwm_limit;
    if (pwm_r >  ctrl->pwm_limit) pwm_r =  ctrl->pwm_limit;
    if (pwm_r < -ctrl->pwm_limit) pwm_r = -ctrl->pwm_limit;

    ctrl->line_left_pwm  = pwm_l;
    ctrl->line_right_pwm = pwm_r;

    DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);
    DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);
}

void MoveControl_Stop(MoveControl_t *ctrl)
{
    if (ctrl == NULL) return;

    if (ctrl->motor_left)  DCMotor_Stop(ctrl->motor_left);
    if (ctrl->motor_right) DCMotor_Stop(ctrl->motor_right);
    ctrl->state = MOVE_STATE_IDLE;
}

uint8_t MoveControl_IsComplete(const MoveControl_t *ctrl)
{
    if (ctrl == NULL) return 1;
    return (ctrl->state == MOVE_STATE_COMPLETE) ? 1 : 0;
}

float MoveControl_GetAvgDistance(const MoveControl_t *ctrl)
{
    if (ctrl == NULL || ctrl->encoder_left == NULL
        || ctrl->encoder_right == NULL) {
        return 0.0f;
    }
    float dis_l = Encoder_GetDistance(ctrl->encoder_left);
    float dis_r = Encoder_GetDistance(ctrl->encoder_right);
    return (dis_l + dis_r) * 0.5f;
}

float MoveControl_GetPositionError(const MoveControl_t *ctrl)
{
    if (ctrl == NULL) return 0.0f;
    return ctrl->target_mm - MoveControl_GetAvgDistance(ctrl);
}

/* ==================== Vofa 上位机调参接口 ==================== */

void MoveControl_SetPosPID(MoveControl_t *ctrl, float kp, float ki, float kd)
{
    if (ctrl == NULL) return;
    ctrl->pos_pid.kp = kp;
    ctrl->pos_pid.ki = ki;
    ctrl->pos_pid.kd = kd;
}

void MoveControl_SetVelPID(MoveControl_t *ctrl, float kp, float ki, float kd)
{
    if (ctrl == NULL) return;
    ctrl->vel_l_pid.kp = kp;
    ctrl->vel_l_pid.ki = ki;
    ctrl->vel_l_pid.kd = kd;
    ctrl->vel_r_pid.kp = kp;
    ctrl->vel_r_pid.ki = ki;
    ctrl->vel_r_pid.kd = kd;
}

void MoveControl_SetBalanceKp(MoveControl_t *ctrl, float kp)
{
    if (ctrl == NULL) return;
    ctrl->balance_kp = kp;
}

void MoveControl_SetBalanceKi(MoveControl_t *ctrl, float ki)
{
    if (ctrl == NULL) return;
    ctrl->balance_ki = ki;
}

void MoveControl_SetBalanceKd(MoveControl_t *ctrl, float kd)
{
    if (ctrl == NULL) return;
    ctrl->balance_kd = kd;
}

/* ==================== 巡线控制接口 ==================== */

void MoveControl_SetLineTrack(MoveControl_t *ctrl, LineSensor_t *sensor)
{
    if (ctrl == NULL || sensor == NULL) return;

    ctrl->line_sensor = sensor;
    ctrl->mode        = MOVE_MODE_LINE_TRACK;
    ctrl->state       = MOVE_STATE_RUNNING;
    ctrl->start_tick  = HAL_GetTick();

    /* 复位巡线遥测数据 */
    ctrl->line_turn      = 0.0f;
    ctrl->line_left_pwm  = 0.0f;
    ctrl->line_right_pwm = 0.0f;
    ctrl->line_ch_bits   = 0;

    /* 复位 PID 历史状态（避免模式切换时残余） */
    PID_Reset(&ctrl->pos_pid);
    PID_Reset(&ctrl->vel_l_pid);
    PID_Reset(&ctrl->vel_r_pid);
    ctrl->balance_integral = 0.0f;
    ctrl->last_dist_diff   = 0.0f;
}

void MoveControl_SetBasePWM(MoveControl_t *ctrl, float pwm)
{
    if (ctrl == NULL) return;
    ctrl->base_pwm = pwm;
}

void MoveControl_SetKLine(MoveControl_t *ctrl, float k)
{
    if (ctrl == NULL) return;
    ctrl->k_line = k;
}

void MoveControl_SetLineWeight(MoveControl_t *ctrl, uint8_t ch, float w)
{
    if (ctrl == NULL || ch >= 8) return;
    ctrl->line_weights[ch] = w;
}
