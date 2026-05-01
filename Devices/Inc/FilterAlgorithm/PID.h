/**
  ******************************************************************************
  * @file    PID.h
  * @brief   PID 控制算法 — 位置式/增量式, 带积分分离、抗饱和、微分滤波等增强
  *
  * @details
  * 本模块不继承 ModuleBase（算法非设备），使用裸结构体 + C 函数封装。
  * 所有 Kp/Ki/Kd 参数为运行时可写字段，便于通过 Vofa/串口上位机在线调参。
  *
  * === 使用示例 ===
  *
  * // 1. 定义并初始化 PID 控制器
  * PID_Controller_t pid_vel;
  * PID_Init(&pid_vel, PID_TYPE_POSITION);
  * PID_SetParams(&pid_vel, 0.15f, 0.10f, 0.02f);
  * PID_SetOutputLimit(&pid_vel, 50.0f, -50.0f);
  * PID_SetIntegralLimit(&pid_vel, 200.0f, -200.0f);
  *
  * // 2. 周期性计算 (25Hz / 40ms)
  * float output = PID_Calculate(target, feedback, &pid_vel);
  *
  * // 3. Vofa 在线调参 (通过串口更新)
  * PID_SetParams(&pid_vel, new_kp, new_ki, new_kd);
  *
  ******************************************************************************
  */

#ifndef __PID_H__
#define __PID_H__

#include <stdint.h>

/* ==================== PID 类型枚举 ==================== */

typedef enum {
    PID_TYPE_POSITION  = 0,  /**< 位置式 PID */
    PID_TYPE_INCREMENT = 1   /**< 增量式 PID */
} PID_Type_e;

/* ==================== PID 控制器结构体 ==================== */

/**
  * @brief PID 控制器结构体
  *
  * 包含 PID 算法的全部参数、状态和增强功能使能标志。
  * Kp/Ki/Kd 为运行时可写字段，不通过宏定义写死。
  */
typedef struct {
    /* ---- PID 核心参数 (运行时可调, Vofa 在线调参) ---- */
    float Kp;               /**< 比例系数 */
    float Ki;               /**< 积分系数 */
    float Kd;               /**< 微分系数 */

    /* ---- 目标与反馈 ---- */
    float target;           /**< 目标值 (设定值) */
    float feedback;         /**< 反馈值 (传感器读数) */
    float last_feedback;    /**< 上次反馈值 (微分先行用) */

    /* ---- 误差状态 ---- */
    float error;            /**< 当前误差 = target - feedback */
    float last_error;       /**< 上次误差 */
    float last_last_error;  /**< 上上次误差 (增量式用) */

    /* ---- 积分 ---- */
    float integral;         /**< 积分累积值 */
    float integral_max;     /**< 积分上限 */
    float integral_min;     /**< 积分下限 */
    float integral_separation_threshold; /**< 积分分离阈值 */

    /* ---- 输出 ---- */
    float output;           /**< 本次 PID 输出值 */
    float output_offset;    /**< 输出前馈/偏移 */
    float output_max;       /**< 输出上限 */
    float output_min;       /**< 输出下限 */
    float last_output;      /**< 上次输出 (增量式用) */

    /* ---- 死区 ---- */
    float deadzone;         /**< 输入死区宽度 */

    /* ---- 控制器类型 ---- */
    PID_Type_e type;        /**< PID 类型: 位置式 / 增量式 */
    uint8_t     first_run;  /**< 增量式首次运行标志 (内部使用) */

    /* ---- 增强功能使能标志 ---- */
    uint8_t enable_integral_separation;   /**< 积分分离 */
    uint8_t enable_integral_limit;        /**< 积分限幅 */
    uint8_t enable_output_limit;          /**< 输出限幅 */
    uint8_t enable_deadzone;              /**< 输入死区 */
    uint8_t enable_derivative_primer;     /**< 微分先行 */
    uint8_t enable_anti_windup;           /**< 抗积分饱和 */
    uint8_t enable_derivative_filter;     /**< 微分滤波 */

    /* ---- 微分滤波 ---- */
    float derivative_filter_alpha;  /**< 一阶低通系数 (0~1, 越小越强) */
    float last_derivative;          /**< 上次微分项 */
    float filtered_derivative;      /**< 滤波后微分项 */
} PID_Controller_t;

/* ==================== 公有接口 ==================== */

/* 初始化与复位 */
void  PID_Init(PID_Controller_t *pid, PID_Type_e type);
void  PID_Reset(PID_Controller_t *pid);

/* 主计算 (设置 target/feedback 并计算输出) */
float PID_Calculate(float target, float feedback, PID_Controller_t *pid);

/* 位置式 / 增量式底层计算 (需先设置 pid->target / pid->feedback) */
float PID_PositionCalculate(PID_Controller_t *pid);
float PID_IncrementCalculate(PID_Controller_t *pid);

/* 目标与反馈设置 */
void  PID_SetTarget(PID_Controller_t *pid, float target);
void  PID_SetFeedback(PID_Controller_t *pid, float feedback);
float PID_GetOutput(const PID_Controller_t *pid);
float PID_GetError(const PID_Controller_t *pid);

/* 参数设置 */
void  PID_SetParams(PID_Controller_t *pid, float kp, float ki, float kd);
void  PID_SetIntegralLimit(PID_Controller_t *pid, float max_val, float min_val);
void  PID_SetOutputLimit(PID_Controller_t *pid, float max_val, float min_val);
void  PID_SetDeadzone(PID_Controller_t *pid, float deadzone);
void  PID_SetIntegralSeparation(PID_Controller_t *pid, float threshold);
void  PID_SetAntiWindup(PID_Controller_t *pid, uint8_t enable);
void  PID_SetDerivativeFilter(PID_Controller_t *pid, uint8_t enable, float alpha);

#endif /* __PID_H__ */
