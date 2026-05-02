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
#include "Gyro.h"
#include <math.h>

/* 默认 PID 参数 (位置环: 距离→RPM) */
#define DEFAULT_POS_KP            1.00f
#define DEFAULT_POS_KI            0.05f
#define DEFAULT_POS_KD            0.06f
#define DEFAULT_POS_INTEGRAL_LIM  200.0f
#define DEFAULT_POS_OUTPUT_LIM    700.0f   /* RPM */

/* 默认 PID 参数 (速度环: RPM→PWM)
   PWM 范围 0~2099, 电机最大转速约 300 RPM,
   kp=5 确保 100RPM 误差 → 500 PWM, kd 提供阻尼防振荡 */
#define DEFAULT_VEL_KP            7.00f
#define DEFAULT_VEL_KI            0.50f
#define DEFAULT_VEL_KD            0.05f
#define DEFAULT_VEL_INTEGRAL_LIM  400.0f
#define DEFAULT_VEL_OUTPUT_LIM    1600.0f  /* PWM */

/* 默认差速修正参数 (PID 控制: mm→RPM, mm/周期→RPM)
   I 项用于消除左右轮速度系统性差异导致的稳态距离偏差 */
#define DEFAULT_BALANCE_KP        6.0f
#define DEFAULT_BALANCE_KI        0.30f
#define DEFAULT_BALANCE_KD        0.05f
#define DEFAULT_BALANCE_INTEGRAL_LIM  300.0f
#define DEFAULT_PWM_LIMIT         1600.0f

/* 控制周期 (秒) */
#define CONTROL_DT                0.04f

/* 停止判定阈值 */
#define STOP_POS_ERROR_MM         20.0f
#define STOP_BASE_VEL_RPM         3.0f

/* 巡线默认参数 */
#define DEFAULT_BASE_PWM          1000.0f
#define DEFAULT_K_LINE            100.0f
#define DEFAULT_LINE_W0           -8.0f
#define DEFAULT_LINE_W1           -2.0f
#define DEFAULT_LINE_W2           -0.30f
#define DEFAULT_LINE_W3           -0.10f
#define DEFAULT_LINE_W4           0.10f /* 0-1 */
#define DEFAULT_LINE_W5           0.30f /* 0-2 */
#define DEFAULT_LINE_W6           2.0f /* 0-8 */
#define DEFAULT_LINE_W7           8.0f /* 0-12 */

/* 巡线状态机默认参数 */
#define DEFAULT_INTERSECTION_THRESHOLD  3       /**< 路口确认连续次数 (3次×2ms=6ms) */
#define DEFAULT_TURN_PWM                800.0f  /**< 转弯基准 PWM */
#define DEFAULT_TURN_TOLERANCE          3.0f    /**< 转弯角度容差 (°) */
#define DEFAULT_TURN_KP                 20.0f   /**< 转弯 P 增益 (PWM/°) */
#define DEFAULT_ADJUST_DISTANCE_MM      80.0f   /**< 微调前进距离 (传感器到轮轴) */
#define DEFAULT_ADJUST_SPEED_PWM        320.0f  /**< 微调前进 PWM */

/* 直线循迹编码器反馈 */
#define DEFAULT_LINE_ENCODER_KP         2.0f    /**< 编码器距离差→PWM 增益 */

/* 转弯后期循迹模块辅助对准 */
#define TURN_LINE_CHECK_YAW_DEG         20.0f   /**< 转弯后期循迹模块介入的 Yaw 误差阈值 (°) */

/* 第 5 段边 (4 次转弯后最终接近) 慢速 PWM，确保精确停车 */
#define DEFAULT_FINAL_SLOW_PWM          320.0f  /**< 最终段慢速循迹 PWM */

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

    /* 巡线状态机默认参数 */
    ctrl->gyro                     = NULL;
    ctrl->line_state               = LINE_STATE_FOLLOWING;
    ctrl->intersection_cnt         = 0;
    ctrl->intersection_threshold   = DEFAULT_INTERSECTION_THRESHOLD;
    ctrl->edge_count               = 0;
    ctrl->target_edges             = 4;
    ctrl->turn_target_yaw          = 0.0f;
    ctrl->turn_start_yaw           = 0.0f;
    ctrl->turn_pwm                 = DEFAULT_TURN_PWM;
    ctrl->turn_tolerance           = DEFAULT_TURN_TOLERANCE;
    ctrl->turn_kp                  = DEFAULT_TURN_KP;
    ctrl->adjust_distance_mm       = DEFAULT_ADJUST_DISTANCE_MM;
    ctrl->adjust_speed_pwm         = DEFAULT_ADJUST_SPEED_PWM;

    ctrl->initial_turn_done = 0;

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
  * @brief  巡线修正更新 (2ms/500Hz, 在 TIM2 ISR 2ms 分频中调用)
  * @note   仅处理 MOVE_MODE_LINE_TRACK 模式，其他模式静默返回。
  *         内部通过 line_state 子状态机调度:
  *           FOLLOWING → INTERSECTION_CONFIRM → FORWARD_ADJUST → TURNING → EDGE_DONE
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

    /* 计算 LineTurn 和 ch_bits (所有状态共用) */
    float line_turn = 0.0f;
    uint8_t ch_bits = 0;
    uint8_t active_cnt = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (ch[i]) {
            line_turn += ctrl->line_weights[i];
            ch_bits |= (1 << i);
            active_cnt++;
        }
    }
    ctrl->line_turn    = line_turn;
    ctrl->line_ch_bits = ch_bits;

    /* ==================== 巡线子状态机 ==================== */

    switch (ctrl->line_state) {

    /* ---- 状态 0: 直线循线 + 路口检测 ---- */
    case LINE_STATE_FOLLOWING: {
        /* 编码器距离差修正: 抑制左右轮行程偏差 */
        float dist_l = 0.0f;
        float dist_r = 0.0f;
        if (ctrl->encoder_left && ctrl->encoder_right) {
            dist_l = Encoder_GetDistance(ctrl->encoder_left);
            dist_r = Encoder_GetDistance(ctrl->encoder_right);
        }
        float dist_diff = dist_r - dist_l;

        /* 加权差速循线 + 编码器平衡修正
           前 4 段边用快速 PWM (1000-1200), 最后一段用慢速 PWM (300) 确保精确停车 */
        float active_base = (ctrl->edge_count >= ctrl->target_edges)
                          ? DEFAULT_FINAL_SLOW_PWM
                          : ctrl->base_pwm;

        float pwm_l = active_base + line_turn * ctrl->k_line
                    - dist_diff * DEFAULT_LINE_ENCODER_KP;
        float pwm_r = active_base - line_turn * ctrl->k_line
                    + dist_diff * DEFAULT_LINE_ENCODER_KP;

        if (pwm_l >  ctrl->pwm_limit) pwm_l =  ctrl->pwm_limit;
        if (pwm_l < -ctrl->pwm_limit) pwm_l = -ctrl->pwm_limit;
        if (pwm_r >  ctrl->pwm_limit) pwm_r =  ctrl->pwm_limit;
        if (pwm_r < -ctrl->pwm_limit) pwm_r = -ctrl->pwm_limit;

        ctrl->line_left_pwm  = pwm_l;
        ctrl->line_right_pwm = pwm_r;

        DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);
        DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);

        /* 路口检测: 一侧 4 路全黑 或 6 路以上全黑 → 疑似路口 */
        uint8_t left4  = (ch[4] && ch[5] && ch[6] && ch[7]);
        uint8_t right4 = (ch[0] && ch[1] && ch[2] && ch[3]);

        if (left4 || right4 || active_cnt >= 6) {
            ctrl->intersection_cnt++;
            if (ctrl->intersection_cnt >= ctrl->intersection_threshold) {
                /* 路口确认 → 停车 */
                DCMotor_Stop(ctrl->motor_left);
                DCMotor_Stop(ctrl->motor_right);

                if (ctrl->edge_count >= ctrl->target_edges) {
                    /* 第 5 次路口 (4 次转弯已完成): 立即停车，不转弯 */
                    ctrl->state = MOVE_STATE_COMPLETE;
                    ctrl->line_state = LINE_STATE_FOLLOWING;
                } else {
                    /* 前 4 次路口: 启动陀螺仪积分, 进入微调前进 → 直角转弯 */
                    if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
                    if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);

                    if (ctrl->gyro != NULL) {
                        Gyro_ResetYaw(ctrl->gyro);
                        Gyro_EnableIntegrate(ctrl->gyro);
                    }

                    ctrl->intersection_cnt = 0;
                    ctrl->line_state = LINE_STATE_FORWARD_ADJUST;
                }
            }
        } else {
            /* 未检测到路口，递减计数 (防噪声累积) */
            if (ctrl->intersection_cnt > 0) {
                ctrl->intersection_cnt--;
            }
        }
        break;
    }

    /* ---- 状态 1: 路口确认 (保留，直接跳到 FORWARD_ADJUST) ---- */
    case LINE_STATE_INTERSECTION_CONFIRM:
        /* 当前逻辑在 FOLLOWING 中直接跳转到 FORWARD_ADJUST，
           此状态保留给未来扩展 (如按键确认路口) */
        ctrl->line_state = LINE_STATE_FORWARD_ADJUST;
        break;

    /* ---- 状态 2: 微调前进 (传感器对齐轮轴中心) ---- */
    case LINE_STATE_FORWARD_ADJUST: {
        float dist = 0.0f;
        if (ctrl->encoder_left && ctrl->encoder_right) {
            float dl = Encoder_GetDistance(ctrl->encoder_left);
            float dr = Encoder_GetDistance(ctrl->encoder_right);
            dist = (dl + dr) * 0.5f;
        }

        if (dist >= ctrl->adjust_distance_mm) {
            /* 微调完成 → 停车, 记录起始 Yaw, 开始转弯 */
            DCMotor_Stop(ctrl->motor_left);
            DCMotor_Stop(ctrl->motor_right);

            if (ctrl->gyro != NULL) {
                ctrl->turn_start_yaw = Gyro_GetYaw(ctrl->gyro);
            } else {
                ctrl->turn_start_yaw = 0.0f;
            }
            ctrl->turn_target_yaw = ctrl->turn_start_yaw + 90.0f;

            /* 清零编码器用于转弯距离参考 */
            if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
            if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);

            ctrl->line_state = LINE_STATE_TURNING;
        } else {
            /* 低速前进 */
            float adj_pwm = ctrl->adjust_speed_pwm;
            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)adj_pwm);
            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)adj_pwm);

            ctrl->line_left_pwm  = adj_pwm;
            ctrl->line_right_pwm = adj_pwm;
        }
        break;
    }

    /* ---- 状态 3: 直角转弯 (陀螺仪 Yaw 闭环 + 循迹模块辅助对准) ---- */
    case LINE_STATE_TURNING: {
        float current_yaw = 0.0f;
        if (ctrl->gyro != NULL) {
            current_yaw = Gyro_GetYaw(ctrl->gyro);
        }

        float yaw_error = ctrl->turn_target_yaw - current_yaw;

        /* 转弯后期 (误差 < 15°): 循迹模块中心通道辅助检测下一段黑线 */
        if (fabsf(yaw_error) < TURN_LINE_CHECK_YAW_DEG) {
            uint8_t center_hit = (ch[2] || ch[3] || ch[4] || ch[5]);
            if (center_hit) {
                /* 中心通道检测到黑线 → 对准完成，提前结束转弯 */
                DCMotor_Stop(ctrl->motor_left);
                DCMotor_Stop(ctrl->motor_right);

                if (ctrl->gyro != NULL) {
                    Gyro_DisableIntegrate(ctrl->gyro);
                    Gyro_ResetYaw(ctrl->gyro);
                }

                ctrl->edge_count++;
                ctrl->line_state = LINE_STATE_EDGE_DONE;
                break;
            }
        }

        if (fabsf(yaw_error) <= ctrl->turn_tolerance) {
            /* 转弯完成 → 停车, 清零 Yaw 防零漂累积, 切换下一边 */
            DCMotor_Stop(ctrl->motor_left);
            DCMotor_Stop(ctrl->motor_right);

            if (ctrl->gyro != NULL) {
                Gyro_DisableIntegrate(ctrl->gyro);
                Gyro_ResetYaw(ctrl->gyro);
            }

            ctrl->edge_count++;
            ctrl->line_state = LINE_STATE_EDGE_DONE;
        } else {
            /* P 控制: 左轮反向、右轮正向 → 逆时针转弯 */
            float turn_out = ctrl->turn_kp * yaw_error;
            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;
            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;

            /* 保证最小转弯 PWM，克服静摩擦 */
            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;
            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;

            float pwm_l = -turn_out;
            float pwm_r =  turn_out;

            ctrl->line_left_pwm  = pwm_l;
            ctrl->line_right_pwm = pwm_r;

            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);
            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);
        }
        break;
    }

    /* ---- 状态 4: 一条边完成 ---- */
    case LINE_STATE_EDGE_DONE:
        /* 停车确保 */
        DCMotor_Stop(ctrl->motor_left);
        DCMotor_Stop(ctrl->motor_right);

        if (ctrl->edge_count > ctrl->target_edges) {
            /* 安全兜底: 超出预期边数 → 强制停止 */
            ctrl->state = MOVE_STATE_COMPLETE;
            ctrl->line_state = LINE_STATE_FOLLOWING;
        } else if (ctrl->edge_count >= ctrl->target_edges) {
            /* 4 次转弯完成，继续循线直行，等待第 5 次路口停车 */
            if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
            if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);

            ctrl->intersection_cnt = 0;
            ctrl->line_state = LINE_STATE_FOLLOWING;
        } else {
            /* 切换下一条边: 清零编码器, 回到循线模式 */
            if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
            if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);

            ctrl->intersection_cnt = 0;
            ctrl->line_state = LINE_STATE_FOLLOWING;
        }
        break;

    /* ---- 状态 5: 初始 90° 转弯 (对齐第一条线) ---- */
    case LINE_STATE_INITIAL_TURN: {
        float current_yaw = 0.0f;
        if (ctrl->gyro != NULL) {
            current_yaw = Gyro_GetYaw(ctrl->gyro);
        }

        /* 若陀螺仪未启动 (无 DMA 数据), 使用固定时长死推算转弯 */
        if (ctrl->gyro != NULL && !Gyro_IsStarted(ctrl->gyro)) {
            /* 等待最多 500ms 让陀螺仪启动 */
            if (HAL_GetTick() - ctrl->start_tick > 500U) {
                /* 超时 → 以固定 PWM 转弯 1.5s 作为死推算 */
                float dead_pwm = ctrl->turn_pwm * 0.6f;
                DCMotor_SetSpeed(ctrl->motor_left,  -(int16_t)dead_pwm);
                DCMotor_SetSpeed(ctrl->motor_right,  (int16_t)dead_pwm);
                ctrl->line_left_pwm  = -dead_pwm;
                ctrl->line_right_pwm =  dead_pwm;

                if (HAL_GetTick() - ctrl->start_tick > 2000U) {
                    DCMotor_Stop(ctrl->motor_left);
                    DCMotor_Stop(ctrl->motor_right);
                    ctrl->initial_turn_done = 1;
                    ctrl->line_state = LINE_STATE_FOLLOWING;
                }
            }
            break;
        }

        float yaw_error = ctrl->turn_target_yaw - current_yaw;

        if (fabsf(yaw_error) <= ctrl->turn_tolerance) {
            /* 初始转弯完成 → 停车, 清零 Yaw, 切换到循线模式 */
            DCMotor_Stop(ctrl->motor_left);
            DCMotor_Stop(ctrl->motor_right);

            if (ctrl->gyro != NULL) {
                Gyro_DisableIntegrate(ctrl->gyro);
                Gyro_ResetYaw(ctrl->gyro);
            }

            ctrl->initial_turn_done = 1;
            ctrl->line_state = LINE_STATE_FOLLOWING;
        } else {
            /* P 控制: 左轮反向、右轮正向 → 逆时针转弯 */
            float turn_out = ctrl->turn_kp * yaw_error;
            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;
            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;

            /* 保证最小转弯 PWM，克服静摩擦 */
            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;
            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;

            float pwm_l = -turn_out;
            float pwm_r =  turn_out;

            ctrl->line_left_pwm  = pwm_l;
            ctrl->line_right_pwm = pwm_r;

            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);
            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);
        }
        break;
    }

    default:
        ctrl->line_state = LINE_STATE_FOLLOWING;
        break;
    }
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

    /* 初始化巡线状态机: 先执行初始 90° 转弯对齐第一条线 */
    ctrl->line_state         = LINE_STATE_INITIAL_TURN;
    ctrl->intersection_cnt   = 0;
    ctrl->edge_count         = 0;
    ctrl->turn_target_yaw    = 90.0f;   /* 目标: 逆时针转 90° */
    ctrl->turn_start_yaw     = 0.0f;

    /* 启用陀螺仪积分用于初始转弯 */
    if (ctrl->gyro != NULL) {
        Gyro_ResetYaw(ctrl->gyro);
        Gyro_EnableIntegrate(ctrl->gyro);
    }

    /* 复位编码器累积 (用于微调前进距离) */
    if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
    if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);

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

/* ==================== 巡线状态机控制接口 ==================== */

/**
  * @brief  绑定陀螺仪到巡线控制器 (转弯 Yaw 闭环)
  */
void MoveControl_SetGyro(MoveControl_t *ctrl, Gyro_t *gyro)
{
    if (ctrl == NULL) return;
    ctrl->gyro = gyro;
}

/**
  * @brief  配置巡线状态机参数 (在 SetLineTrack 之前调用)
  * @param  ctrl                 运动控制器
  * @param  target_edges         目标边数 (4=一圈)
  * @param  intersect_threshold  路口确认连续次数 (默认 5)
  * @param  turn_pwm             转弯基准 PWM (默认 400)
  * @param  adjust_mm            微调前进距离 (传感器到轮轴, mm)
  */
void MoveControl_SetLineTrackConfig(MoveControl_t *ctrl,
                                    uint8_t target_edges,
                                    uint8_t intersect_threshold,
                                    float turn_pwm,
                                    float adjust_mm)
{
    if (ctrl == NULL) return;
    ctrl->target_edges           = target_edges;
    ctrl->intersection_threshold = intersect_threshold;
    ctrl->turn_pwm               = turn_pwm;
    ctrl->adjust_distance_mm     = adjust_mm;
}

/**
  * @brief  复位巡线状态机 (重新开始循迹)
  */
void MoveControl_ResetLineTrack(MoveControl_t *ctrl)
{
    if (ctrl == NULL) return;

    ctrl->line_state         = LINE_STATE_INITIAL_TURN;
    ctrl->intersection_cnt   = 0;
    ctrl->edge_count         = 0;
    ctrl->turn_target_yaw    = 90.0f;
    ctrl->turn_start_yaw     = 0.0f;
    ctrl->initial_turn_done  = 0;
    ctrl->state              = MOVE_STATE_RUNNING;

    if (ctrl->gyro != NULL) {
        Gyro_ResetYaw(ctrl->gyro);
        Gyro_EnableIntegrate(ctrl->gyro);
    }
    if (ctrl->encoder_left)  Encoder_ClearData(ctrl->encoder_left);
    if (ctrl->encoder_right) Encoder_ClearData(ctrl->encoder_right);
}

/**
  * @brief  获取已完成的边数
  */
uint8_t MoveControl_GetEdgeCount(const MoveControl_t *ctrl)
{
    if (ctrl == NULL) return 0;
    return ctrl->edge_count;
}

/**
  * @brief  查询循迹是否完成 (所有边均已走过)
  * @retval 1  已完成
  * @retval 0  未完成
  */
uint8_t MoveControl_GetLineTrackDone(const MoveControl_t *ctrl)
{
    if (ctrl == NULL) return 1;
    return (ctrl->state == MOVE_STATE_COMPLETE) ? 1 : 0;
}
