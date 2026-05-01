/**
  ******************************************************************************
  * @file    StepperMotor.h
  * @brief   步进电机子类 — 继承 MotorBase 的 28BYJ-48 步进电机控制
  *
  * @details
  * 本文件定义了 StepperMotor 结构体和公有接口，继承自 MotorBase 基类。
  * 子类负责管理：
  *   - 4 相引脚配置（通过结构体数组传入）
  *   - 3 种步进模式（半步八拍/整步四拍/波驱动四拍）
  *   - 角度/脉冲数双跟踪
  *   - DWT 周期计数器驱动的梯形加减速控制 (微秒级)
  *
  * === 步进模式 ===
  *
  * | 模式              | 相电流 | 每周期步数 | 力矩   | 每转步数 |
  * |-------------------|--------|-----------|--------|---------|
  * | STEP_MODE_HALF_8  | 1→2交替| 8         | 平滑   | 4096    |
  * | STEP_MODE_FULL_4  | 2      | 4         | 较高   | 2048    |
  * | STEP_MODE_WAVE_4  | 1      | 4         | 最低   | 2048    |
  *
  * === 梯形加减速 ===
  *
  * 每次运动计算三段：加速段 → 匀速段(巡航) → 减速段。
  * 短距离运动自动跳过巡航段，减速从部分加速直接转入。
  * 加速和减速步数默认各占总步数的 1/3，最多各不超过 1/2。
  *
  * === 使用示例 ===
  *
  * // 1. 定义引脚配置
  * static const StepperPinConfig pins[4] = {
  *     {GPIOA, GPIO_PIN_8},   // IN1
  *     {GPIOA, GPIO_PIN_9},   // IN2
  *     {GPIOA, GPIO_PIN_10},  // IN3
  *     {GPIOA, GPIO_PIN_11},  // IN4
  * };
  *
  * // 2. 构造并初始化
  * StepperMotor motor;
  * StepperMotor_Constructor(&motor, "Step1", pins);
  * Motor_Init(&motor.base);
  *
  * // 3. 设置角度（触发梯形加减速运动）
  * StepperMotor_SetAngle(&motor, 9000);   // 旋转到 90.00°
  *
  * // 4. 主循环中运行（检测 step_flag 并执行步进）
  * Motor_Run(&motor.base);
  *
  ******************************************************************************
  */

#ifndef __STEPPER_MOTOR_H__
#define __STEPPER_MOTOR_H__

/* Includes ------------------------------------------------------------------*/
#include "MotorBase.h"
#include "gpio.h"
#include <stdint.h>

/* ==================== 步进电机常量 ==================== */

#define STEPPER_STEPS_PER_REV_HALF  4096   /**< 半步模式每转步数 (64*64=4096) */
#define STEPPER_DEGREES_PER_REV       360  /**< 一圈对应的角度 */
#define STEPPER_ANGLE_SCALE           100  /**< 角度定点小数比例 (0.01° 分辨率) */

/* ==================== 步进模式枚举 ==================== */

typedef enum {
    STEP_MODE_HALF_8 = 0,  /**< 半步八拍 (默认, 力矩平滑, 4096步/转) */
    STEP_MODE_FULL_4 = 1,  /**< 整步四拍 (高力矩, 2048步/转) */
    STEP_MODE_WAVE_4 = 2,  /**< 波驱动四拍 (低力矩, 2048步/转) */
} StepperStepMode;

/* ==================== 引脚配置结构体 ==================== */

typedef struct {
    GPIO_TypeDef *port;    /**< GPIO 端口 */
    uint16_t      pin;     /**< GPIO 引脚 */
} StepperPinConfig;

/* ==================== 梯形加减速阶段枚举 ==================== */

typedef enum {
    TRAPEZOID_ACCEL = 0,   /**< 加速段 */
    TRAPEZOID_CRUISE = 1,  /**< 匀速段 */
    TRAPEZOID_DECEL = 2,   /**< 减速段 */
    TRAPEZOID_DONE   = 3,  /**< 运动完成 */
} TrapezoidPhase;

/* ==================== 步进电机结构体定义 ==================== */

typedef struct StepperMotor_s {
    MotorBase                base;              /**< 基类 (必须为第一个成员) */
    const StepperPinConfig  *pins;              /**< 4 相引脚配置数组指针 */
    StepperStepMode          step_mode;         /**< 当前步进模式 */
    uint8_t                  step_index;        /**< 当前在拍序表中的位置 (0~7) */

    /* === 角度与脉冲追踪 === */
    int32_t                  target_angle;      /**< 目标角度 (定点小数, 0.01° 单位) */
    int32_t                  current_angle;     /**< 当前角度 (定点小数, 0.01° 单位) */
    int32_t                  target_pulses;     /**< 目标总脉冲数 */
    int32_t                  current_pulses;    /**< 当前脉冲计数 */

    /* === 转速 === */
    uint8_t                  speed_grade;       /**< 转速等级 (1~10) */

    /* === 梯形加减速参数 === */
    uint32_t                 accel_steps;       /**< 加速段步数 */
    uint32_t                 cruise_steps;      /**< 匀速段步数 */
    uint32_t                 decel_steps;       /**< 减速段步数 */
    uint32_t                 total_steps;       /**< 本次运动总步数 */
    uint32_t                 phase_step_count;  /**< 当前段已走步数 */
    TrapezoidPhase           trap_phase;        /**< 当前梯形段 */

    /* === DWT 步进时序 (微秒级, 非阻塞) === */
    uint32_t                 step_interval_us;  /**< 当前步间间隔 (us) */
    uint32_t                 last_step_tick;    /**< 上一步的 DWT 时间戳 (us) */

    /* === 方向与状态 === */
    uint8_t                  direction;         /**< 方向: 0=正转(角度增加), 1=反转 */
} StepperMotor_t;

/* ==================== 公有接口函数 ==================== */

void StepperMotor_Constructor(StepperMotor_t *self, const char *name,
                              const StepperPinConfig *pins);
void StepperMotor_SetAngle(StepperMotor_t *self, int32_t angle);
void StepperMotor_ResetAngle(StepperMotor_t *self);
void StepperMotor_SetMode(StepperMotor_t *self, StepperStepMode mode);
void StepperMotor_SetSpeed(StepperMotor_t *self, uint8_t grade);
int32_t StepperMotor_GetAngle(const StepperMotor_t *self);
int32_t StepperMotor_GetPulses(const StepperMotor_t *self);
void StepperMotor_EmergencyStop(StepperMotor_t *self);

#endif /* __STEPPER_MOTOR_H__ */
