/**
  ******************************************************************************
  * @file    Encoder.h
  * @brief   编码器传感器子类 — 继承 SensorBase 的霍尔编码器实现
  *
  * @details
  * 本文件定义了编码器模块的结构体和公有接口，继承自 SensorBase 基类。
  * 基类提供 name、initialized、update_period_ms 等属性，子类专注实现：
  *   - 脉冲计数（基于 TIM 编码器模式，四倍频）
  *   - 转速计算（RPM）
  *   - 线速度计算（mm/s）
  *   - 行驶距离累积（mm）
  *   - 数据清零与硬件复位
  *
  * === 硬件配置（来自 Config.md）===
  *
  * 编码器线数:     ENCODER_LINE = 13
  * 减速比:         GEAR_RATIO   = 20.409f
  * 四倍频单圈脉冲: PULSE_PER_ROUND = ENCODER_LINE * 4 * GEAR_RATIO
  * 车轮直径:       WHEEL_DIAMETER = 65.0f (mm)
  * 车轮周长:       WHEEL_CIRCUMFERENCE = WHEEL_DIAMETER * π
  *
  * 左电机编码器:   TIM1_CH1 (PE9) + TIM1_CH2 (PE11)
  * 右电机编码器:   TIM8_CH1 (PC6) + TIM8_CH2 (PC7)
  *
  * === 使用示例 ===
  *
  * // 1. 定义并构造编码器对象
  * Encoder_t left_enc;
  * Encoder_Constructor(&left_enc, &htim1, 0);  // 左电机, TIM1
  *
  * // 2. 通过基类接口初始化
  * SensorBase_Init((SensorBase_t *)&left_enc);
  *
  * // 3. 周期性更新（在定时中断中调用，周期 40ms）
  * SensorBase_Run((SensorBase_t *)&left_enc);
  *
  * // 4. 读取编码器数据
  * float rpm  = Encoder_GetRPM(&left_enc);
  * float mmps = Encoder_GetMMPS(&left_enc);
  * float dist = Encoder_GetDistance(&left_enc);
  *
  * // 5. 转弯后仅清零累积数据（不重置硬件计数器）
  * Encoder_ClearData(&left_enc);
  *
  * // 6. 完全复位（含硬件计数器清零）
  * SensorBase_Reset((SensorBase_t *)&left_enc);
  * SensorBase_Cleanup((SensorBase_t *)&left_enc);
  *
  ******************************************************************************
  */

#ifndef __ENCODER_H__
#define __ENCODER_H__

/* Includes ------------------------------------------------------------------*/
#include "SensorBase.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "DebugPrintf.h"

/* ==================== 编码器配置宏（来自 Config.md）==================== */

/**
  * @defgroup Encoder_Config 编码器硬件配置宏
  * @{
  */
#define ENCODER_LINE            13              /**< 霍尔编码器磁环线数 */
#define GEAR_RATIO              20.409f         /**< 电机减速比 */
#define PULSE_PER_ROUND         (ENCODER_LINE * 4 * GEAR_RATIO) /**< 四倍频后单圈脉冲数 */
#define WHEEL_DIAMETER          65.0f           /**< 车轮直径 (mm) */
#define WHEEL_CIRCUMFERENCE     (WHEEL_DIAMETER * 3.1415926f)   /**< 车轮周长 (mm) */
#define WHEEL_BASE_DISTANCE     125.0f          /**< 轮基距离 (mm) */
/**
  * @}
  */

/* ==================== 编码器结构体定义 ==================== */

/**
  * @brief 编码器模块结构体（继承 SensorBase）
  *
  * @note   SensorBase_t 必须为第一个成员，确保指针可安全转换
  */
typedef struct Encoder_s {
    SensorBase_t       base;             /**< 基类（必须为第一个成员）*/
    TIM_HandleTypeDef  *tim_handle;      /**< TIM 句柄指针（编码器模式）*/
    uint8_t            motor_index;      /**< 电机索引 (0=左, 1=右) */

    /* 脉冲计数 */
    int16_t            current_cnt;      /**< 当前 TIM 计数器值 */
    int16_t            last_cnt;         /**< 上次 TIM 计数器值 */
    int32_t            pulse_diff;       /**< 本次脉冲差（带符号）*/
    int64_t            total_pulse;      /**< 累计脉冲数 */

    /* 运动数据 */
    float              rpm;              /**< 当前转速 (转/分钟) */
    float              mmps;             /**< 当前线速度 (毫米/秒) */
    float              distance_mm;      /**< 累积行驶距离 (毫米) */

    /* 打印速率限制 */
    uint32_t           last_print_tick;  /**< 上次打印时刻 (HAL_GetTick)，用于 0.5s 节流 */
} Encoder_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  编码器构造函数
  * @note   初始化基类成员，设置 TIM 句柄和电机索引。
  *         构造后默认清零所有数据字段。
  * @param  self         指向编码器对象的指针
  * @param  tim_handle   编码器模式 TIM 句柄指针
  * @param  motor_index  电机索引（0=左电机, 1=右电机）
  */
void Encoder_Constructor(Encoder_t *self, TIM_HandleTypeDef *tim_handle, uint8_t motor_index);

/**
  * @brief  获取当前转速
  * @param  self  指向编码器对象的指针
  * @return float 转速 (RPM)，参数无效时返回 0.0f
  */
float Encoder_GetRPM(const Encoder_t *self);

/**
  * @brief  获取当前线速度
  * @param  self  指向编码器对象的指针
  * @return float 线速度 (mm/s)，参数无效时返回 0.0f
  */
float Encoder_GetMMPS(const Encoder_t *self);

/**
  * @brief  获取累积行驶距离
  * @param  self  指向编码器对象的指针
  * @return float 累积距离 (mm)，参数无效时返回 0.0f
  */
float Encoder_GetDistance(const Encoder_t *self);

/**
  * @brief  获取累计脉冲数
  * @param  self  指向编码器对象的指针
  * @return int64_t 累计脉冲数，参数无效时返回 0
  */
int64_t Encoder_GetTotalPulse(const Encoder_t *self);

/**
  * @brief  获取本次脉冲差
  * @param  self  指向编码器对象的指针
  * @return int32_t 本轮脉冲差（带符号），参数无效时返回 0
  */
int32_t Encoder_GetPulseDiff(const Encoder_t *self);

/**
  * @brief  清零所有累积数据（保留硬件计数器运行）
  * @note   用于多段运动场景，如转弯后重新开始距离计数。
  *         仅清零 total_pulse、distance_mm 等软件数据，
  *         不复位 TIM 硬件计数器。
  * @param  self  指向编码器对象的指针
  */
void Encoder_ClearData(Encoder_t *self);

/**
  * @brief  完全复位编码器数据（含硬件计数器）
  * @note   清零所有软件数据并将 TIM 计数器归零。
  *         通常在系统初始化或紧急停止时使用。
  * @param  self  指向编码器对象的指针
  */
void Encoder_HardReset(Encoder_t *self);

/**
  * @brief  打印编码器运动信息（通过 DebugPrintf DMA 发送）
  * @note   内置 0.5s 速率限制（per-instance），避免刷屏。
  *         单行格式示例：
  *         "[L] RPM=123.4 Speed=456.7mm/s Dist=100.0mm Pulse=500 Diff=10"
  * @param  self    指向编码器对象的指针（NULL 安全）
  * @param  dbg     指向 DebugPrintf 对象的指针（NULL 安全，NULL 时不输出）
  * @param  label   标签字符（如 'L'=左, 'R'=右），显示在行首
  * @param  is_last 是否为最后一行（0=末尾加 \r\n, 1=不加）
  */
void Encoder_PrintInfo(Encoder_t *self, DebugPrintf_t *dbg, char label, uint8_t is_last);

/**
  * @brief  打印双编码器运动信息（便捷函数）
  * @note   内置 0.5s 速率限制（基于左编码器的 last_print_tick）。
  *         输出格式（共 3 行，最后一行无尾随 \r\n）：
  *         "=== Encoder t=0.500s ===\r\n"
  *         "[L] RPM=123.4 Speed=456.7mm/s Dist=100.0mm Pulse=500 Diff=10\r\n"
  *         "[R] RPM=122.0 Speed=450.0mm/s Dist=98.0mm Pulse=490 Diff=9"
  * @param  left  左编码器指针（NULL 安全）
  * @param  right 右编码器指针（NULL 安全）
  * @param  dbg   DebugPrintf 对象指针（NULL 安全）
  */
void Encoder_PrintDualInfo(Encoder_t *left, Encoder_t *right, DebugPrintf_t *dbg);

#endif /* __ENCODER_H__ */
