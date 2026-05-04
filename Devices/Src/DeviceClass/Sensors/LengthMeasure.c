/**
  ******************************************************************************
  * @file    LengthMeasure.c
  * @brief   长度测量模块实现 — "丰"字图案白色间隙测量
  *
  * @details
  * 8 状态跳变检测状态机：
  *   IDLE → WAIT_TAPE1 → WAIT_GAP1 → WAIT_TAPE2 → WAIT_GAP2 → WAIT_TAPE3
  *   → WAIT_EXIT → DONE
  *
  * 跳变条件：
  *   - TAPE 状态 → 下一状态: active_channels >= 5 (黑胶带)
  *   - GAP  状态 → 下一状态: active_channels <= 2 (白纸)
  *
  * 跳变6 (WAIT_EXIT→DONE): 退出第三条黑胶带进入白纸，pattern_exit_flag 置位，
  * 用于触发减速带 PWM 恢复。
  *
  * 防抖：连续 N 次 (默认 1×1ms=1ms) 满足条件才触发跳变。
  * 超时：行驶距离超过 timeout_distance_mm (默认 400mm) 后退出。
  ******************************************************************************
  */

#include "LengthMeasure.h"

/* ==================== 跳变阈值宏 ==================== */

#define TAPE_THRESHOLD  5   /**< >= 此值认为是黑胶带 */
#define GAP_THRESHOLD   2   /**< <= 此值认为是白纸 */
#define DEFAULT_CONFIRM 1   /**< 默认防抖次数 (1×2ms=2ms) */
#define DEFAULT_TIMEOUT_MM 400.0f  /**< 默认超时距离 */

/* ==================== 私有辅助 ==================== */

/** @brief  获取左右编码器平均距离 (mm) */
static float LengthMeasure_GetAvgDist(const LengthMeasure_t *self)
{
    float dl = Encoder_GetDistance(self->enc_l);
    float dr = Encoder_GetDistance(self->enc_r);
    return (dl + dr) * 0.5f;
}

/* ==================== 公有接口实现 ==================== */

void LengthMeasure_Constructor(LengthMeasure_t *self,
                               LineSensor_t *line_sensor,
                               Encoder_t *enc_l, Encoder_t *enc_r)
{
    if (self == NULL) return;

    self->line_sensor       = line_sensor;
    self->enc_l             = enc_l;
    self->enc_r             = enc_r;

    self->state             = LENGTH_MEASURE_IDLE;
    self->confirm_cnt       = 0;
    self->confirm_threshold = DEFAULT_CONFIRM;
    self->start_distance_mm = 0.0f;
    self->timeout_distance_mm = DEFAULT_TIMEOUT_MM;
    self->done_flag         = 0;
    self->timeout_flag      = 0;
    self->pattern_exit_flag = 0;

    for (uint8_t i = 0; i < 4; i++) {
        self->enc_reading[i] = 0.0f;
    }
    self->L1_mm = 0.0f;
    self->L2_mm = 0.0f;
}

void LengthMeasure_Arm(LengthMeasure_t *self)
{
    if (self == NULL) return;
    if (self->state != LENGTH_MEASURE_IDLE) return;

    self->state             = LENGTH_MEASURE_WAIT_TAPE1;
    self->confirm_cnt       = 0;
    self->done_flag         = 0;
    self->timeout_flag      = 0;
    self->pattern_exit_flag = 0;
    self->start_distance_mm = LengthMeasure_GetAvgDist(self);

    for (uint8_t i = 0; i < 4; i++) {
        self->enc_reading[i] = 0.0f;
    }
    self->L1_mm = 0.0f;
    self->L2_mm = 0.0f;
}

void LengthMeasure_Run(LengthMeasure_t *self)
{
    if (self == NULL || self->line_sensor == NULL) return;
    if (self->enc_l == NULL || self->enc_r == NULL) return;

    LengthMeasureState_t s = self->state;

    /* IDLE/DONE/TIMEOUT → 空操作 */
    if (s == LENGTH_MEASURE_IDLE
        || s == LENGTH_MEASURE_DONE
        || s == LENGTH_MEASURE_TIMEOUT) {
        return;
    }

    uint8_t active = LineSensor_GetActiveChannels(self->line_sensor);
    float dist = LengthMeasure_GetAvgDist(self);

    /* 超时检查 */
    float traveled = dist - self->start_distance_mm;
    if (traveled > self->timeout_distance_mm) {
        self->state        = LENGTH_MEASURE_TIMEOUT;
        self->timeout_flag = 1;
        return;
    }

    /* 状态机 */
    switch (s) {

    case LENGTH_MEASURE_WAIT_TAPE1:
        if (active >= TAPE_THRESHOLD) {
            self->confirm_cnt++;
            if (self->confirm_cnt >= self->confirm_threshold) {
                self->state       = LENGTH_MEASURE_WAIT_GAP1;
                self->confirm_cnt = 0;
            }
        } else {
            self->confirm_cnt = 0;
        }
        break;

    case LENGTH_MEASURE_WAIT_GAP1:
        if (active <= GAP_THRESHOLD) {
            self->confirm_cnt++;
            if (self->confirm_cnt >= self->confirm_threshold) {
                self->enc_reading[0] = dist;
                self->state          = LENGTH_MEASURE_WAIT_TAPE2;
                self->confirm_cnt    = 0;
            }
        } else {
            self->confirm_cnt = 0;
        }
        break;

    case LENGTH_MEASURE_WAIT_TAPE2:
        if (active >= TAPE_THRESHOLD) {
            self->confirm_cnt++;
            if (self->confirm_cnt >= self->confirm_threshold) {
                self->enc_reading[1] = dist;
                self->L1_mm = self->enc_reading[1] - self->enc_reading[0];
                self->state       = LENGTH_MEASURE_WAIT_GAP2;
                self->confirm_cnt = 0;
            }
        } else {
            self->confirm_cnt = 0;
        }
        break;

    case LENGTH_MEASURE_WAIT_GAP2:
        if (active <= GAP_THRESHOLD) {
            self->confirm_cnt++;
            if (self->confirm_cnt >= self->confirm_threshold) {
                self->enc_reading[2] = dist;
                self->state          = LENGTH_MEASURE_WAIT_TAPE3;
                self->confirm_cnt    = 0;
            }
        } else {
            self->confirm_cnt = 0;
        }
        break;

    case LENGTH_MEASURE_WAIT_TAPE3:
        if (active >= TAPE_THRESHOLD) {
            self->confirm_cnt++;
            if (self->confirm_cnt >= self->confirm_threshold) {
                self->enc_reading[3] = dist;
                self->L2_mm = self->enc_reading[3] - self->enc_reading[2];
                self->state       = LENGTH_MEASURE_WAIT_EXIT;
                self->confirm_cnt = 0;
            }
        } else {
            self->confirm_cnt = 0;
        }
        break;

    case LENGTH_MEASURE_WAIT_EXIT:
        /* 跳变6: 退出第三条黑胶带进入白纸 → pattern_exit_flag 置位 */
        if (active <= GAP_THRESHOLD) {
            self->confirm_cnt++;
            if (self->confirm_cnt >= self->confirm_threshold) {
                self->pattern_exit_flag = 1;
                self->state     = LENGTH_MEASURE_DONE;
                self->done_flag = 1;
            }
        } else {
            self->confirm_cnt = 0;
        }
        break;

    default:
        break;
    }
}

void LengthMeasure_SetDebounce(LengthMeasure_t *self, uint8_t threshold)
{
    if (self == NULL) return;
    self->confirm_threshold = threshold;
}

void LengthMeasure_SetTimeout(LengthMeasure_t *self, float distance_mm)
{
    if (self == NULL) return;
    self->timeout_distance_mm = distance_mm;
}

uint8_t LengthMeasure_IsDone(const LengthMeasure_t *self)
{
    if (self == NULL) return 0;
    return self->done_flag;
}

uint8_t LengthMeasure_IsTimeout(const LengthMeasure_t *self)
{
    if (self == NULL) return 0;
    return self->timeout_flag;
}

uint8_t LengthMeasure_IsPatternExit(const LengthMeasure_t *self)
{
    if (self == NULL) return 0;
    return self->pattern_exit_flag;
}

float LengthMeasure_GetL1Cm(const LengthMeasure_t *self)
{
    if (self == NULL) return 0.0f;
    return self->L1_mm / 10.0f;
}

float LengthMeasure_GetL2Cm(const LengthMeasure_t *self)
{
    if (self == NULL) return 0.0f;
    return self->L2_mm / 10.0f;
}

void LengthMeasure_Reset(LengthMeasure_t *self)
{
    if (self == NULL) return;

    self->state             = LENGTH_MEASURE_IDLE;
    self->confirm_cnt       = 0;
    self->start_distance_mm = 0.0f;
    self->done_flag         = 0;
    self->timeout_flag      = 0;
    self->pattern_exit_flag = 0;

    for (uint8_t i = 0; i < 4; i++) {
        self->enc_reading[i] = 0.0f;
    }
    self->L1_mm = 0.0f;
    self->L2_mm = 0.0f;
}
