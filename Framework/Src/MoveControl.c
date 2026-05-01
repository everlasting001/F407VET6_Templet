#include "MoveControl.h"
#include <math.h>
#include <stdlib.h>

/* ==================== 静态辅助函数 ==================== */

/**
  * @brief  将 PID 浮点输出转换为电机 PWM 值, 并送入电机
  */
static void MoveControl_ApplyPWM(MoveControl_t *mc, float pwm_l, float pwm_r) {
    int16_t left  = (int16_t)pwm_l;
    int16_t right = (int16_t)pwm_r;

    /* 死区: 低于最低运行值的 PWM 电机无法响应 */
    if (abs(left)  < mc->pwm_min_run) left  = 0;
    if (abs(right) < mc->pwm_min_run) right = 0;

    /* 限幅 */
    if (left  >  mc->pwm_max) left  =  mc->pwm_max;
    if (left  < -mc->pwm_max) left  = -mc->pwm_max;
    if (right >  mc->pwm_max) right =  mc->pwm_max;
    if (right < -mc->pwm_max) right = -mc->pwm_max;

    mc->pwm_left  = left;
    mc->pwm_right = right;

    DCMotor_SetSpeed(mc->motor_left,  left);
    DCMotor_SetSpeed(mc->motor_right, right);
}

/* ==================== 初始化 ==================== */

/**
  * @brief  初始化运动控制器
  *
  * 绑定编码器和电机, 初始化三个 PID 控制器为位置式并设置默认参数。
  * 默认参数为经验参考值, 实际需根据硬件和工况通过 Vofa 在线调参优化。
  *
  * @param  mc     运动控制器指针
  * @param  enc_l  左轮编码器指针
  * @param  enc_r  右轮编码器指针
  * @param  mot_l  左轮电机指针
  * @param  mot_r  右轮电机指针
  */
void MoveControl_Init(MoveControl_t *mc,
                      Encoder_t *enc_l, Encoder_t *enc_r,
                      DCMotor_t *mot_l, DCMotor_t *mot_r)
{
    /* 硬件绑定 */
    mc->enc_left   = enc_l;
    mc->enc_right  = enc_r;
    mc->motor_left = mot_l;
    mc->motor_right = mot_r;

    /* 位置环 PID (位置式) */
    PID_Init(&mc->pid_pos, PID_TYPE_POSITION);
    PID_SetParams(&mc->pid_pos, 0.17f, 0.10f, 0.10f);
    PID_SetOutputLimit(&mc->pid_pos, 50.0f, -50.0f);
    PID_SetIntegralLimit(&mc->pid_pos, 200.0f, -200.0f);
    PID_SetDeadzone(&mc->pid_pos, 5.0f);
    PID_SetIntegralSeparation(&mc->pid_pos, 50.0f);
    PID_SetDerivativeFilter(&mc->pid_pos, 1, 0.5f);

    /* 左轮速度环 PID (位置式) */
    PID_Init(&mc->pid_vel_l, PID_TYPE_POSITION);
    PID_SetParams(&mc->pid_vel_l, 0.15f, 0.10f, 0.02f);
    PID_SetOutputLimit(&mc->pid_vel_l, 50.0f, -50.0f);
    PID_SetIntegralLimit(&mc->pid_vel_l, 200.0f, -200.0f);
    PID_SetIntegralSeparation(&mc->pid_vel_l, 200.0f);
    PID_SetDerivativeFilter(&mc->pid_vel_l, 1, 0.7f);

    /* 右轮速度环 PID (位置式) */
    PID_Init(&mc->pid_vel_r, PID_TYPE_POSITION);
    PID_SetParams(&mc->pid_vel_r, 0.15f, 0.10f, 0.02f);
    PID_SetOutputLimit(&mc->pid_vel_r, 50.0f, -50.0f);
    PID_SetIntegralLimit(&mc->pid_vel_r, 200.0f, -200.0f);
    PID_SetIntegralSeparation(&mc->pid_vel_r, 200.0f);
    PID_SetDerivativeFilter(&mc->pid_vel_r, 1, 0.7f);

    /* 差速修正参数 */
    mc->balance_kp = 0.5f;
    mc->balance_kd = 0.2f;

    /* 控制状态 */
    mc->control_mode = MOVE_MODE_STOP;
    mc->target_distance_mm = 0.0f;
    mc->target_speed_mmps  = 0.0f;
    mc->last_dist_diff = 0.0f;
    mc->pwm_left  = 0;
    mc->pwm_right = 0;
    mc->arrived   = 0;

    /* 输出限幅 */
    mc->pwm_max     = 50;
    mc->pwm_min_run = 10;

    /* 到达阈值 */
    mc->pos_stop_threshold = 5.0f;
    mc->vel_stop_threshold = 10.0f;
}

/* ==================== 控制模式设置 ==================== */

/**
  * @brief  设置定位置控制目标
  * @note   设置后自动从当前位置开始计算距离偏差。
  *         如需从零开始, 先调用 Encoder_ClearData() 清零编码器。
  * @param  mc          运动控制器指针
  * @param  distance_mm  目标距离 (mm), 正=前进, 负=后退
  */
void MoveControl_SetDistanceTarget(MoveControl_t *mc, float distance_mm) {
    mc->target_distance_mm = distance_mm;
    mc->control_mode = MOVE_MODE_POSITION;
    mc->arrived = 0;

    /* 复位位置 PID 状态 */
    PID_Reset(&mc->pid_pos);

    /* 复位速度 PID 状态 */
    PID_Reset(&mc->pid_vel_l);
    PID_Reset(&mc->pid_vel_r);

    /* 清零差速历史 */
    mc->last_dist_diff = 0.0f;
}

/**
  * @brief  设置定速控制目标
  * @param  mc           运动控制器指针
  * @param  speed_mmps   目标线速度 (mm/s), 正=前进, 负=后退
  */
void MoveControl_SetSpeedTarget(MoveControl_t *mc, float speed_mmps) {
    mc->target_speed_mmps = speed_mmps;
    mc->control_mode = MOVE_MODE_SPEED;
    mc->arrived = 0;

    PID_Reset(&mc->pid_vel_l);
    PID_Reset(&mc->pid_vel_r);
    mc->last_dist_diff = 0.0f;
}

/**
  * @brief  停止运动
  */
void MoveControl_Stop(MoveControl_t *mc) {
    mc->control_mode = MOVE_MODE_STOP;
    mc->arrived = 1;

    DCMotor_Stop(mc->motor_left);
    DCMotor_Stop(mc->motor_right);

    mc->pwm_left  = 0;
    mc->pwm_right = 0;

    PID_Reset(&mc->pid_pos);
    PID_Reset(&mc->pid_vel_l);
    PID_Reset(&mc->pid_vel_r);
}

/* ==================== 核心控制更新 ==================== */

/**
  * @brief  运动控制主更新函数
  *
  * 应在主循环中由 Flag_40ms (25Hz) 触发调用, 不在 ISR 中直接调用。
  *
  * === 位置模式控制流程 ===
  *   1. 读取左右编码器距离和速度
  *   2. 位置环 PID: 平均距离误差 → 基础速度
  *   3. 差速修正: 左右距离差 → 转向修正量
  *   4. 速度环 PID: 左轮(基础+修正) → PWM, 右轮(基础-修正) → PWM
  *   5. 到达判定: 距离误差 < 阈值 且 速度 < 阈值 → 停止
  *
  * === 速度模式控制流程 ===
  *   1. 读取左右编码器速度
  *   2. 差速修正: 左右距离漂移 → 修正量
  *   3. 速度环 PID: 左轮(目标+修正) → PWM, 右轮(目标-修正) → PWM
  *
  * @param  mc  运动控制器指针
  */
void MoveControl_Update(MoveControl_t *mc) {
    if (mc->control_mode == MOVE_MODE_STOP) {
        return;
    }

    /* 安全校验: 硬件绑定指针非空 */
    if (!mc->enc_left || !mc->enc_right || !mc->motor_left || !mc->motor_right) {
        return;
    }

    /* === 1. 读取编码器反馈 === */
    float dis_l  = Encoder_GetDistance(mc->enc_left);
    float dis_r  = Encoder_GetDistance(mc->enc_right);
    float vel_l  = Encoder_GetMMPS(mc->enc_left);
    float vel_r  = Encoder_GetMMPS(mc->enc_right);
    float avg_dis = (dis_l + dis_r) * 0.5f;

    /* 差速修正: 右轮多走为正, 说明车右偏 */
    float dist_diff   = dis_r - dis_l;
    float dist_diff_d = dist_diff - mc->last_dist_diff;
    mc->last_dist_diff = dist_diff;

    float turn_correction = dist_diff * mc->balance_kp
                          + dist_diff_d * mc->balance_kd;

    float target_vel_l, target_vel_r;

    if (mc->control_mode == MOVE_MODE_POSITION) {
        /* === 2. 位置模式: 级联控制 === */

        /* 2a. 位置环: 距离误差 → 基础速度 */
        float base_velocity = PID_Calculate(mc->target_distance_mm, avg_dis,
                                            &mc->pid_pos);

        /* 2b. 组合目标速度: 基础速度 ± 差速修正 */
        target_vel_l = base_velocity + turn_correction;
        target_vel_r = base_velocity - turn_correction;

        /* 2c. 速度环: 速度 → PWM */
        float pwm_l = PID_Calculate(target_vel_l, vel_l, &mc->pid_vel_l);
        float pwm_r = PID_Calculate(target_vel_r, vel_r, &mc->pid_vel_r);

        MoveControl_ApplyPWM(mc, pwm_l, pwm_r);

        /* 2d. 到达判定 */
        float pos_error = mc->target_distance_mm - avg_dis;
        if (fabsf(pos_error) < mc->pos_stop_threshold
            && fabsf(base_velocity) < mc->vel_stop_threshold) {
            MoveControl_Stop(mc);
        }

    } else if (mc->control_mode == MOVE_MODE_SPEED) {
        /* === 3. 速度模式 === */

        /* 3a. 组合目标速度: 目标速度 ± 差速修正 */
        target_vel_l = mc->target_speed_mmps + turn_correction;
        target_vel_r = mc->target_speed_mmps - turn_correction;

        /* 3b. 速度环: 速度 → PWM */
        float pwm_l = PID_Calculate(target_vel_l, vel_l, &mc->pid_vel_l);
        float pwm_r = PID_Calculate(target_vel_r, vel_r, &mc->pid_vel_r);

        MoveControl_ApplyPWM(mc, pwm_l, pwm_r);
    }
}

/* ==================== PID 参数设置 ==================== */

/**
  * @brief  设置位置环 PID 参数 (Kp/Ki/Kd 运行时可调, Vofa 在线调参)
  */
void MoveControl_SetPosPID(MoveControl_t *mc, float kp, float ki, float kd) {
    PID_SetParams(&mc->pid_pos, kp, ki, kd);
}

/**
  * @brief  统一设置左/右速度环 PID 参数 (Kp/Ki/Kd 运行时可调, Vofa 在线调参)
  */
void MoveControl_SetVelPID(MoveControl_t *mc, float kp, float ki, float kd) {
    PID_SetParams(&mc->pid_vel_l, kp, ki, kd);
    PID_SetParams(&mc->pid_vel_r, kp, ki, kd);
}

/**
  * @brief  设置差速修正的 Kp/Kd
  */
void MoveControl_SetBalanceGain(MoveControl_t *mc, float kp, float kd) {
    mc->balance_kp = kp;
    mc->balance_kd = kd;
}

/* ==================== 状态查询 ==================== */

uint8_t MoveControl_HasArrived(const MoveControl_t *mc) {
    return mc->arrived;
}

float MoveControl_GetAvgDistance(const MoveControl_t *mc) {
    if (!mc->enc_left || !mc->enc_right) {
        return 0.0f;
    }
    float dis_l = Encoder_GetDistance(mc->enc_left);
    float dis_r = Encoder_GetDistance(mc->enc_right);
    return (dis_l + dis_r) * 0.5f;
}

float MoveControl_GetAvgSpeed(const MoveControl_t *mc) {
    if (!mc->enc_left || !mc->enc_right) {
        return 0.0f;
    }
    float vel_l = Encoder_GetMMPS(mc->enc_left);
    float vel_r = Encoder_GetMMPS(mc->enc_right);
    return (vel_l + vel_r) * 0.5f;
}

int16_t MoveControl_GetPWMLeft(const MoveControl_t *mc) {
    return mc->pwm_left;
}

int16_t MoveControl_GetPWMRight(const MoveControl_t *mc) {
    return mc->pwm_right;
}

