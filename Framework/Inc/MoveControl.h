/**
  ******************************************************************************
  * @file    MoveControl.h
  * @brief   小车直行运动控制 — 级联 PID (位置环 + 速度环 + 差速修正)
  *
  * @details
  * 本模块位于 Framework 层，组合 Encoder(反馈) + DCMotor(执行) + PID(算法)
  * 实现两种控制模式:
  *   - 定位置控制 (Position Mode):  外环位置 PID → 内环速度 PID → PWM
  *   - 定速控制   (Speed Mode):    速度 PID → PWM
  *
  * 差速修正: 通过左右轮行进距离差计算转向修正量, 防止小车跑偏。
  *
  * === 控制周期 ===
  * MoveControl_Update() 应在主循环中调用, 由 Flag_40ms 触发 (25Hz)。
  * 不在 ISR 中直接调用, 避免阻塞中断。
  *
  * === 级联结构 (位置模式) ===
  *
  *   target_distance ──→ [位置 PID] ──→ base_velocity ──┬──→ [左轮速度 PID] ──→ pwm_left
  *                      (pid_pos)                       │     (pid_vel_l)
  *                                                     ├──→ [右轮速度 PID] ──→ pwm_right
  *                              turn_correction ───────┘     (pid_vel_r)
  *                                (差速修正)
  *
  * === 使用示例 ===
  *
  *   // 1. 定义并初始化
  *   MoveControl_t move_ctrl;
  *   MoveControl_Init(&move_ctrl, &enc_left, &enc_right, &motor_left, &motor_right);
  *
  *   // 2. 设置 PID 参数 (Kp/Ki/Kd 均为运行时可调, Vofa 在线调参)
  *   MoveControl_SetPosPID(&move_ctrl, 0.17f, 0.10f, 0.10f);
  *   MoveControl_SetVelPID(&move_ctrl, 0.15f, 0.10f, 0.02f);
  *   MoveControl_SetBalanceGain(&move_ctrl, 0.5f, 0.2f);
  *
  *   // 3. 设置目标并开始
  *   MoveControl_SetDistanceTarget(&move_ctrl, 600.0f);  // 前进 600mm
  *
  *   // 4. 主循环中周期更新 (25Hz)
  *   if (Flag_40ms) {
  *       Flag_40ms = 0;
  *       MoveControl_Update(&move_ctrl);
  *       if (MoveControl_HasArrived(&move_ctrl)) {
  *           // 到达目标, 停止
  *       }
  *   }
  *
  ******************************************************************************
  */

#ifndef __MOVE_CONTROL_H__
#define __MOVE_CONTROL_H__

#include "PID.h"
#include "Encoder.h"
#include "DCMotor.h"
#include <stdint.h>


/* ==================== 控制模式枚举 ==================== */

typedef enum {
    MOVE_MODE_STOP     = 0,  /**< 停止 */
    MOVE_MODE_POSITION = 1,  /**< 定位置控制 (距离) */
    MOVE_MODE_SPEED    = 2   /**< 定速控制 (速度) */
} MoveMode_e;

/* ==================== 运动控制结构体 ==================== */

/**
  * @brief 运动控制结构体
  *
  * 组合编码器、电机、PID 控制器, 实现闭环直行控制。
  * 所有 PID Kp/Ki/Kd 参数均为运行时可写, 支持 Vofa 在线调参。
  */
typedef struct {
    /* ---- 硬件绑定 ---- */
    Encoder_t  *enc_left;    /**< 左轮编码器指针 */
    Encoder_t  *enc_right;   /**< 右轮编码器指针 */
    DCMotor_t  *motor_left;  /**< 左轮电机指针 */
    DCMotor_t  *motor_right; /**< 右轮电机指针 */

    /* ---- PID 控制器 (三个) ---- */
    PID_Controller_t pid_pos;    /**< 位置环 (外环): 距离误差 → 基础速度 */
    PID_Controller_t pid_vel_l;  /**< 左轮速度环 (内环): 速度 → PWM */
    PID_Controller_t pid_vel_r;  /**< 右轮速度环 (内环): 速度 → PWM */

    /* ---- 差速修正参数 ---- */
    float balance_kp;       /**< 差速修正比例系数 */
    float balance_kd;       /**< 差速修正微分系数 */

    /* ---- 目标与模式 ---- */
    MoveMode_e control_mode;    /**< 当前控制模式 */
    float      target_distance_mm;  /**< 目标距离 (mm), 位置模式 */
    float      target_speed_mmps;   /**< 目标速度 (mm/s), 速度模式 */

    /* ---- 运行状态 ---- */
    float   last_dist_diff;   /**< 上次左右轮距离差 (用于微分) */
    int16_t pwm_left;         /**< 左轮 PWM 输出值 */
    int16_t pwm_right;        /**< 右轮 PWM 输出值 */
    uint8_t arrived;          /**< 到达标志 */

    /* ---- 输出限幅 ---- */
    int16_t pwm_max;          /**< PWM 输出上限 */
    int16_t pwm_min_run;      /**< PWM 最低运行值 (低于此值电机不转) */

    /* ---- 到达判定阈值 ---- */
    float pos_stop_threshold;   /**< 位置模式停止距离阈值 (mm) */
    float vel_stop_threshold;   /**< 位置模式停止速度阈值 (mm/s) */
} MoveControl_t;

/* ==================== 公有接口 ==================== */

/* 初始化与更新 */
void MoveControl_Init(MoveControl_t *mc,
                      Encoder_t *enc_l, Encoder_t *enc_r,
                      DCMotor_t *mot_l, DCMotor_t *mot_r);
void MoveControl_Update(MoveControl_t *mc);

/* 控制模式设置 */
void MoveControl_SetDistanceTarget(MoveControl_t *mc, float distance_mm);
void MoveControl_SetSpeedTarget(MoveControl_t *mc, float speed_mmps);
void MoveControl_Stop(MoveControl_t *mc);

/* PID 参数设置 (Kp/Ki/Kd 均为运行时可调, Vofa 在线调参) */
void MoveControl_SetPosPID(MoveControl_t *mc, float kp, float ki, float kd);
void MoveControl_SetVelPID(MoveControl_t *mc, float kp, float ki, float kd);
void MoveControl_SetBalanceGain(MoveControl_t *mc, float kp, float kd);

/* 状态查询 */
uint8_t MoveControl_HasArrived(const MoveControl_t *mc);
float   MoveControl_GetAvgDistance(const MoveControl_t *mc);
float   MoveControl_GetAvgSpeed(const MoveControl_t *mc);
int16_t MoveControl_GetPWMLeft(const MoveControl_t *mc);
int16_t MoveControl_GetPWMRight(const MoveControl_t *mc);

#endif /* __MOVE_CONTROL_H__ */
