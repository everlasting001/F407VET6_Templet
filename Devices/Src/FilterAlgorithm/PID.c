#include "PID.h"
#include <math.h>

/* ==================== 静态辅助函数声明 ==================== */

static float  PID_DeadzoneProcess(float error, PID_Controller_t *pid);
static uint8_t PID_IntegralSeparationCheck(float error, PID_Controller_t *pid);
static void   PID_IntegralLimitProcess(PID_Controller_t *pid);
static void   PID_OutputLimitProcess(PID_Controller_t *pid);
static void   PID_AntiWindupProcess(PID_Controller_t *pid);
static float  PID_DerivativeFilter(float derivative, PID_Controller_t *pid);
static float  PID_DerivativePrimer(PID_Controller_t *pid);

/* ==================== 初始化与复位 ==================== */

/**
  * @brief  初始化 PID 控制器
  * @param  pid   控制器指针
  * @param  type  控制器类型 (位置式 / 增量式)
  */
void PID_Init(PID_Controller_t *pid, PID_Type_e type) {
    pid->Kp = 0.0f;
    pid->Ki = 0.0f;
    pid->Kd = 0.0f;

    pid->target        = 0.0f;
    pid->feedback      = 0.0f;
    pid->last_feedback = 0.0f;
    pid->error         = 0.0f;
    pid->last_error    = 0.0f;
    pid->last_last_error = 0.0f;

    pid->integral      = 0.0f;
    pid->integral_max  = 0.0f;
    pid->integral_min  = 0.0f;
    pid->integral_separation_threshold = 0.0f;

    pid->output        = 0.0f;
    pid->output_offset = 0.0f;
    pid->output_max    = 100.0f;
    pid->output_min    = -100.0f;
    pid->last_output   = 0.0f;

    pid->deadzone      = 0.0f;

    pid->type      = type;
    pid->first_run = 1;

    pid->enable_integral_separation = 0;
    pid->enable_integral_limit      = 0;
    pid->enable_output_limit        = 0;
    pid->enable_deadzone            = 0;
    pid->enable_derivative_primer   = 0;
    pid->enable_anti_windup         = 1;
    pid->enable_derivative_filter   = 1;

    pid->derivative_filter_alpha = 0.75f;
    pid->last_derivative         = 0.0f;
    pid->filtered_derivative     = 0.0f;
}

/**
  * @brief  复位 PID 控制器 (仅清零运行状态, 保留参数配置)
  * @param  pid  控制器指针
  */
void PID_Reset(PID_Controller_t *pid) {
    pid->error           = 0.0f;
    pid->last_error      = 0.0f;
    pid->last_last_error = 0.0f;
    pid->integral        = 0.0f;
    pid->output          = 0.0f;
    pid->last_output     = 0.0f;
    pid->last_derivative = 0.0f;
    pid->filtered_derivative = 0.0f;
    pid->first_run       = 1;
}

/* ==================== 增强功能实现 ==================== */

/**
  * @brief  死区处理: |error| < deadzone 时返回 0, 否则减掉死区
  */
static float PID_DeadzoneProcess(float error, PID_Controller_t *pid) {
    if (!pid->enable_deadzone) {
        return error;
    }

    float abs_err = fabsf(error);
    if (abs_err < pid->deadzone) {
        return 0.0f;
    }

    return (error > 0.0f) ? (error - pid->deadzone)
                          : (error + pid->deadzone);
}

/**
  * @brief  积分分离检查: |error| < threshold 时允许积分
  * @return 1=允许积分, 0=禁止积分
  */
static uint8_t PID_IntegralSeparationCheck(float error, PID_Controller_t *pid) {
    if (!pid->enable_integral_separation) {
        return 1;
    }
    return (fabsf(error) < pid->integral_separation_threshold) ? 1 : 0;
}

/**
  * @brief  积分限幅: 将 integral 钳位到 [integral_min, integral_max]
  */
static void PID_IntegralLimitProcess(PID_Controller_t *pid) {
    if (!pid->enable_integral_limit) {
        return;
    }
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < pid->integral_min) {
        pid->integral = pid->integral_min;
    }
}

/**
  * @brief  输出限幅: 将 output 钳位到 [output_min, output_max]
  */
static void PID_OutputLimitProcess(PID_Controller_t *pid) {
    if (!pid->enable_output_limit) {
        return;
    }
    if (pid->output > pid->output_max) {
        pid->output = pid->output_max;
    } else if (pid->output < pid->output_min) {
        pid->output = pid->output_min;
    }
}

/**
  * @brief  抗积分饱和: 当输出饱和且误差方向会加剧饱和时, 衰减积分
  */
static void PID_AntiWindupProcess(PID_Controller_t *pid) {
    if (!pid->enable_anti_windup || !pid->enable_output_limit) {
        return;
    }
    if ((pid->output >= pid->output_max && pid->error > 0.0f) ||
        (pid->output <= pid->output_min && pid->error < 0.0f)) {
        pid->integral *= 0.95f;
    }
}

/**
  * @brief  微分滤波: 一阶低通 IIR 滤波器
  *         filtered = alpha * raw + (1-alpha) * last_filtered
  */
static float PID_DerivativeFilter(float derivative, PID_Controller_t *pid) {
    if (!pid->enable_derivative_filter) {
        return derivative;
    }
    pid->filtered_derivative = pid->derivative_filter_alpha * derivative
                             + (1.0f - pid->derivative_filter_alpha) * pid->last_derivative;
    pid->last_derivative = pid->filtered_derivative;
    return pid->filtered_derivative;
}

/**
  * @brief  微分先行: 对反馈值微分而非对误差微分
  *         derivative = -Kd * (feedback - last_feedback)
  *         可避免目标突变时的微分冲击
  */
static float PID_DerivativePrimer(PID_Controller_t *pid) {
    float actual_diff = pid->feedback - pid->last_feedback;
    return -(pid->Kd * actual_diff);
}

/* ==================== PID 计算核心 ==================== */

/**
  * @brief  位置式 PID 计算
  *         output = Kp*e + Ki*∫e + Kd*(e - e_last) + offset
  */
float PID_PositionCalculate(PID_Controller_t *pid) {
    float proportional, integral_term, differential;

    /* 1. 误差计算 + 死区 */
    pid->error = pid->target - pid->feedback;
    pid->error = PID_DeadzoneProcess(pid->error, pid);

    /* 2. 比例项 */
    proportional = pid->Kp * pid->error;

    /* 3. 积分项 — 检查积分分离后累加, 再限幅 */
    if (PID_IntegralSeparationCheck(pid->error, pid)) {
        /* 预判: 若即将饱和且误差加剧, 阻止本次累加 */
        float tentative = proportional + pid->Ki * (pid->integral + pid->error)
                        + pid->output_offset;
        uint8_t would_saturate = 0;
        if (pid->enable_anti_windup && pid->enable_output_limit) {
            would_saturate = (tentative >= pid->output_max && pid->error > 0.0f)
                          || (tentative <= pid->output_min && pid->error < 0.0f);
        }
        if (!would_saturate) {
            pid->integral += pid->error;
        }
        PID_IntegralLimitProcess(pid);
    }
    integral_term = pid->Ki * pid->integral;

    /* 4. 微分项 — 可选微分先行, 然后滤波 */
    float raw_derivative;
    if (pid->enable_derivative_primer) {
        raw_derivative = PID_DerivativePrimer(pid);
    } else {
        raw_derivative = pid->Kd * (pid->error - pid->last_error);
    }
    differential = PID_DerivativeFilter(raw_derivative, pid);

    /* 5. 合成输出 */
    pid->output = proportional + integral_term + differential + pid->output_offset;

    /* 6. 输出限幅 */
    PID_OutputLimitProcess(pid);

    /* 7. 饱和后衰减积分 */
    PID_AntiWindupProcess(pid);

    /* 8. 保存历史 */
    pid->last_error    = pid->error;
    pid->last_feedback = pid->feedback;

    return pid->output;
}

/**
  * @brief  增量式 PID 计算
  *         Δu = Kp*(e0-e1) + Ki*e0 + Kd*(e0 - 2*e1 + e2)
  *         output = last_output + Δu
  */
float PID_IncrementCalculate(PID_Controller_t *pid) {
    float delta_output;

    /* 1. 误差计算 + 死区 */
    pid->error = pid->target - pid->feedback;
    pid->error = PID_DeadzoneProcess(pid->error, pid);

    /* 2. 增量式计算 */
    float p_delta = pid->Kp * (pid->error - pid->last_error);

    float i_delta = 0.0f;
    if (PID_IntegralSeparationCheck(pid->error, pid)) {
        i_delta = pid->Ki * pid->error;
    }

    float d_raw = pid->Kd * (pid->error - 2.0f * pid->last_error + pid->last_last_error);
    float d_delta = PID_DerivativeFilter(d_raw, pid);

    delta_output = p_delta + i_delta + d_delta;

    /* 3. 累加输出 (首次加入偏移) */
    if (pid->first_run) {
        pid->output    = pid->last_output + delta_output + pid->output_offset;
        pid->first_run = 0;
    } else {
        pid->output = pid->last_output + delta_output;
    }

    /* 4. 抗饱和：若输出已饱和且增量加剧饱和，回退 */
    if (pid->enable_anti_windup && pid->enable_output_limit) {
        if ((pid->output >= pid->output_max && delta_output > 0.0f) ||
            (pid->output <= pid->output_min && delta_output < 0.0f)) {
            pid->output = pid->last_output;
        }
    }

    /* 5. 输出限幅 */
    PID_OutputLimitProcess(pid);

    /* 6. 保存历史 */
    pid->last_last_error = pid->last_error;
    pid->last_error      = pid->error;
    pid->last_output     = pid->output;

    return pid->output;
}

/**
  * @brief  PID 主计算函数 — 设置 target/feedback 并计算
  * @param  target    目标值
  * @param  feedback  反馈值
  * @param  pid       PID 控制器指针
  * @return float     控制输出
  */
float PID_Calculate(float target, float feedback, PID_Controller_t *pid) {
    pid->target   = target;
    pid->feedback = feedback;

    if (pid->type == PID_TYPE_POSITION) {
        return PID_PositionCalculate(pid);
    } else {
        return PID_IncrementCalculate(pid);
    }
}

/* ==================== 参数设置 ==================== */

void PID_SetTarget(PID_Controller_t *pid, float target) {
    pid->target = target;
}

void PID_SetFeedback(PID_Controller_t *pid, float feedback) {
    pid->feedback = feedback;
}

float PID_GetOutput(const PID_Controller_t *pid) {
    return pid->output;
}

float PID_GetError(const PID_Controller_t *pid) {
    return pid->error;
}

void PID_SetParams(PID_Controller_t *pid, float kp, float ki, float kd) {
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
}

void PID_SetIntegralLimit(PID_Controller_t *pid, float max_val, float min_val) {
    pid->integral_max = max_val;
    pid->integral_min = min_val;
    pid->enable_integral_limit = 1;
}

void PID_SetOutputLimit(PID_Controller_t *pid, float max_val, float min_val) {
    pid->output_max = max_val;
    pid->output_min = min_val;
    pid->enable_output_limit = 1;
}

void PID_SetDeadzone(PID_Controller_t *pid, float deadzone) {
    pid->deadzone = deadzone;
    pid->enable_deadzone = (deadzone > 0.0f) ? 1 : 0;
}

void PID_SetIntegralSeparation(PID_Controller_t *pid, float threshold) {
    pid->integral_separation_threshold = threshold;
    pid->enable_integral_separation = (threshold > 0.0f) ? 1 : 0;
}

void PID_SetAntiWindup(PID_Controller_t *pid, uint8_t enable) {
    pid->enable_anti_windup = enable;
}

void PID_SetDerivativeFilter(PID_Controller_t *pid, uint8_t enable, float alpha) {
    pid->enable_derivative_filter = enable;
    if (alpha > 0.0f && alpha < 1.0f) {
        pid->derivative_filter_alpha = alpha;
    }
}
