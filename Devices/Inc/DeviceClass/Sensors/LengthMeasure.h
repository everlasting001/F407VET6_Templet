/**
  ******************************************************************************
  * @file    LengthMeasure.h
  * @brief   长度测量模块 — "丰"字图案白色间隙测量
  *
  * @details
  * 轻量级独立模块（不继承 SensorBase），消费 LineSensor 和 Encoder 的已有数据，
  * 通过 6 状态跳变检测状态机测量 Task3 减速带区域"丰"字图案的两个白色间隙 L1、L2。
  *
  * === 图案结构 ===
  *   黑色胶带1 (1.8cm) → 白色间隙 L1 → 黑色胶带2 (1.8cm)
  *   → 白色间隙 L2 → 黑色胶带3 (1.8cm)
  *
  * === 跳变阈值 ===
  *   黑色胶带: 灰度传感器 >= 5 路亮
  *   白色纸张: 灰度传感器 <= 2 路亮
  *
  * === 使用示例 ===
  *
  *   LengthMeasure_t lm;
  *   LengthMeasure_Constructor(&lm, &line_sensor, &left_encoder, &right_encoder);
  *   LengthMeasure_Arm(&lm);  // 进入减速带时调用
  *
  *   // 在 2ms ISR 中:
  *   LengthMeasure_Run(&lm);
  *
  *   // 在主循环中:
  *   if (LengthMeasure_IsDone(&lm)) {
  *       float L1 = LengthMeasure_GetL1Cm(&lm);
  *       float L2 = LengthMeasure_GetL2Cm(&lm);
  *   }
  *
  ******************************************************************************
  */

#ifndef __LENGTH_MEASURE_H__
#define __LENGTH_MEASURE_H__

#include "LineSensor.h"
#include "Encoder.h"

/* ==================== 状态枚举 ==================== */

typedef enum {
    LENGTH_MEASURE_IDLE       = 0,  /**< 未武装，Run() 空操作 */
    LENGTH_MEASURE_WAIT_TAPE1 = 1,  /**< 武装，等待第一条黑胶带 (>=5) */
    LENGTH_MEASURE_WAIT_GAP1  = 2,  /**< 在黑胶带上，等待第一个白间隙 (<=2) */
    LENGTH_MEASURE_WAIT_TAPE2 = 3,  /**< 在白间隙中，等待第二条黑胶带 (>=5) */
    LENGTH_MEASURE_WAIT_GAP2  = 4,  /**< 在黑胶带上，等待第二个白间隙 (<=2) */
    LENGTH_MEASURE_WAIT_TAPE3 = 5,  /**< 在白间隙中，等待第三条黑胶带 (>=5) */
    LENGTH_MEASURE_DONE       = 6,  /**< 测量完成，L1/L2 有效 */
    LENGTH_MEASURE_TIMEOUT    = 7   /**< 超时退出 */
} LengthMeasureState_t;

/* ==================== 结构体定义 ==================== */

typedef struct {
    /* 状态机 */
    LengthMeasureState_t  state;

    /* 传感器引用 (只读，不拥有) */
    LineSensor_t         *line_sensor;
    Encoder_t            *enc_l;
    Encoder_t            *enc_r;

    /* 测量数据 */
    float                 enc_reading[4];      /**< 4 次跳变的编码器读数 (mm) */
    float                 L1_mm;               /**< 白色间隙 L1 长度 (mm) */
    float                 L2_mm;               /**< 白色间隙 L2 长度 (mm) */

    /* 防抖 */
    uint8_t               confirm_cnt;         /**< 连续满足条件的计数 */
    uint8_t               confirm_threshold;   /**< 跳变确认阈值 (默认 3, =6ms) */

    /* 超时保护 */
    float                 start_distance_mm;   /**< 武装时的编码器距离 (mm) */
    float                 timeout_distance_mm; /**< 最大允许行驶距离 (mm, 默认 400) */

    /* 输出标志 (ISR 设置, 主循环读取) */
    volatile uint8_t      done_flag;
    volatile uint8_t      timeout_flag;
} LengthMeasure_t;

/* ==================== 公有接口 ==================== */

void     LengthMeasure_Constructor(LengthMeasure_t *self,
                                   LineSensor_t *line_sensor,
                                   Encoder_t *enc_l, Encoder_t *enc_r);
void     LengthMeasure_Arm(LengthMeasure_t *self);
void     LengthMeasure_Run(LengthMeasure_t *self);
void     LengthMeasure_SetDebounce(LengthMeasure_t *self, uint8_t threshold);
void     LengthMeasure_SetTimeout(LengthMeasure_t *self, float distance_mm);
uint8_t  LengthMeasure_IsDone(const LengthMeasure_t *self);
uint8_t  LengthMeasure_IsTimeout(const LengthMeasure_t *self);
float    LengthMeasure_GetL1Cm(const LengthMeasure_t *self);
float    LengthMeasure_GetL2Cm(const LengthMeasure_t *self);
void     LengthMeasure_Reset(LengthMeasure_t *self);

#endif /* __LENGTH_MEASURE_H__ */
