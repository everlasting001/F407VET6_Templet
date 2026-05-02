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

/* 默认差速修正参数 (PD 控制: mm→RPM, mm/周期→RPM) */
#define DEFAULT_BALANCE_KP        1.6f
#define DEFAULT_BALANCE_KD        0.02f
#define DEFAULT_PWM_LIMIT         1000.0f

/* 控制周期 (秒) */
#define CONTROL_DT                0.04f

/* 停止判定阈值 */
#define STOP_POS_ERROR_MM         1.0f
#define STOP_BASE_VEL_RPM         3.0f

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
    ctrl->balance_kd     = DEFAULT_BALANCE_KD;
    ctrl->last_dist_diff = 0.0f;
    ctrl->pwm_limit      = DEFAULT_PWM_LIMIT;

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
    ctrl->last_dist_diff = 0.0f;

    ctrl->mode           = MOVE_MODE_POSITION;
    ctrl->target_mm      = target_mm;
    ctrl->start_tick     = HAL_GetTick();
    ctrl->state          = MOVE_STATE_RUNNING;
}

void MoveControl_Update(MoveControl_t *ctrl)
{
    if (ctrl == NULL
        || ctrl->motor_left == NULL || ctrl->motor_right == NULL
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

    /* ---- 4. 差速修正 PD：dist_diff*Kp + derivative*Kd → 转向修正量 (RPM) ---- */
    float dist_diff = dis_r - dis_l;
    float derivative = dist_diff - ctrl->last_dist_diff;
    ctrl->last_dist_diff = dist_diff;
    float turn_correction = dist_diff * ctrl->balance_kp
                          + derivative * ctrl->balance_kd;

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

void MoveControl_SetBalanceKd(MoveControl_t *ctrl, float kd)
{
    if (ctrl == NULL) return;
    ctrl->balance_kd = kd;
}
