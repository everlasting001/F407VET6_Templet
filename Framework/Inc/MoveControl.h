/**
  ******************************************************************************
  * @file    MoveControl.h
  * @brief   运动控制系统 — 级联 PID 双轮差速控制（Framework 层）
  *
  * @details
  * 算法参考 Algorithm.md 的 GoStraight_Control 三环级联结构：
  *   1. 位置环 PID：平均距离 → 基础速度 (RPM)
  *   2. 差速修正 PD：dist_diff*Kp + derivative*Kd → 转向修正量 (RPM)
  *   3. 速度环 PID×2：左右轮独立计算 PWM
  *
  * === 控制周期 ===
  * 本模块由 Framework_IRQHandler() 在 40ms (25Hz) 定时调用，与编码器更新同步。
  *
  * === 使用示例 ===
  *
  *   MoveControl_t mc;
  *   MoveControl_Init(&mc, &left_motor, &right_motor,
  *                    &left_encoder, &right_encoder);
  *   MoveControl_SetTarget(&mc, 1000.0f);
  *   // 在 40ms ISR 中调用: MoveControl_Update(&mc);
  *
  ******************************************************************************
  */

#ifndef __MOVE_CONTROL_H__
#define __MOVE_CONTROL_H__

#include "DCMotor.h"
#include "Encoder.h"
#include "PID.h"
#include "LineSensor.h"

typedef enum {
    MOVE_STATE_IDLE     = 0,
    MOVE_STATE_RUNNING  = 1,
    MOVE_STATE_COMPLETE = 2
} MoveState_t;

typedef enum {
    MOVE_MODE_POSITION    = 0,
    MOVE_MODE_SPEED       = 1,
    MOVE_MODE_LINE_TRACK  = 2
} MoveMode_t;

typedef struct {
    /* 设备引用 */
    DCMotor_t  *motor_left;
    DCMotor_t  *motor_right;
    Encoder_t  *encoder_left;
    Encoder_t  *encoder_right;

    /* 目标参数 */
    MoveMode_t   mode;
    float        target_mm;          /**< 目标距离 (mm)，位置模式 */
    float        target_speed_mmps;  /**< 目标速度 (mm/s)，速度模式(预留) */

    /* 级联 PID (Kp/Ki/Kd 为运行时字段，后续 Vofa 上位机调参) */
    PID_t        pos_pid;            /**< 位置环: 距离误差 → 基础速度 (RPM) */
    PID_t        vel_l_pid;          /**< 左轮速度环: 速度误差 → PWM */
    PID_t        vel_r_pid;          /**< 右轮速度环: 速度误差 → PWM */

    /* 差速修正参数 (PD→PID，I 项消除左右轮稳态偏差) */
    float        balance_kp;         /**< 差速修正比例系数 (mm→RPM) */
    float        balance_ki;         /**< 差速修正积分系数 (mm*s→RPM) */
    float        balance_kd;         /**< 差速修正微分系数 (mm/周期→RPM) */
    float        balance_integral;   /**< 差速修正积分累加 (mm*s) */
    float        last_dist_diff;     /**< 上一周期位移差 (用于微分计算) */

    /* 输出限幅 */
    float        pwm_limit;          /**< PWM 输出限幅 (绝对值) */

    /* 巡线控制参数 (MOVE_MODE_LINE_TRACK) */
    LineSensor_t *line_sensor;       /**< 灰度传感器引用 */
    float         base_pwm;          /**< 巡线基准 PWM (600~1000) */
    float         k_line;            /**< LineTurn→PWM 增益 (默认 200) */
    float         line_weights[8];   /**< 8 通道权重 (Vofa 可调) */
    float         line_turn;         /**< 当前 LineTurn (Vofa 遥测) */
    float         line_left_pwm;     /**< 当前左轮 PWM (Vofa 遥测) */
    float         line_right_pwm;    /**< 当前右轮 PWM (Vofa 遥测) */
    uint8_t       line_ch_bits;      /**< 当前通道二进制值 (Vofa 遥测) */

    /* 运行状态 */
    MoveState_t  state;
    uint32_t     start_tick;         /**< 运动开始时刻 (ms) */
} MoveControl_t;

/* 初始化 */
void     MoveControl_Init(MoveControl_t *ctrl,
                          DCMotor_t *motor_left, DCMotor_t *motor_right,
                          Encoder_t *encoder_left, Encoder_t *encoder_right);

/* 设置目标距离并启动 */
void     MoveControl_SetTarget(MoveControl_t *ctrl, float target_mm);

/* 控制更新 (每 40ms 调用一次，由 Framework_IRQHandler 驱动，位置/速度模式) */
void     MoveControl_Update(MoveControl_t *ctrl);

/* 巡线修正更新 (5ms 周期, 200Hz, 由 TIM2 ISR 5ms 分频驱动) */
void     MoveControl_LineTrackUpdate(MoveControl_t *ctrl);

/* 启动巡线模式 */
void     MoveControl_SetLineTrack(MoveControl_t *ctrl, LineSensor_t *sensor);

/* 巡线参数调整 (Vofa 上位机调参接口) */
void     MoveControl_SetBasePWM(MoveControl_t *ctrl, float pwm);
void     MoveControl_SetKLine(MoveControl_t *ctrl, float k);
void     MoveControl_SetLineWeight(MoveControl_t *ctrl, uint8_t ch, float w);

/* 紧急停止 */
void     MoveControl_Stop(MoveControl_t *ctrl);

/* 状态查询 */
uint8_t  MoveControl_IsComplete(const MoveControl_t *ctrl);
float    MoveControl_GetAvgDistance(const MoveControl_t *ctrl);
float    MoveControl_GetPositionError(const MoveControl_t *ctrl);

/* 参数运行时调整 (Vofa 上位机调参接口) */
void     MoveControl_SetPosPID(MoveControl_t *ctrl, float kp, float ki, float kd);
void     MoveControl_SetVelPID(MoveControl_t *ctrl, float kp, float ki, float kd);
void     MoveControl_SetBalanceKp(MoveControl_t *ctrl, float kp);
void     MoveControl_SetBalanceKi(MoveControl_t *ctrl, float ki);
void     MoveControl_SetBalanceKd(MoveControl_t *ctrl, float kd);

#endif /* __MOVE_CONTROL_H__ */
