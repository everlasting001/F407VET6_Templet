/**
  ******************************************************************************
  * @file    MotorBase.h
  * @brief   电机模块基类 — 所有电机类型（DC、步进）的纯虚基类接口
  *
  * @details
  * 本文件定义了所有电机类型共有的最小接口。
  * 设计原则：只保留所有电机真正共有的属性和方法，类似 ModuleBase。
  * 引脚定义（DC 的方向/编码器 vs 步进的四相）由各自子类管理。
  *
  * === 使用示例 ===
  *
  * // 1. 子类继承 MotorBase（作为第一个成员）
  * typedef struct {
  *     MotorBase base;
  *     // ... 子类自有引脚和属性
  * } DCMotor;
  *
  * // 2. 实现子类虚函数
  * static int DCMotor_init(void *self) { ... }
  * static int DCMotor_run(void *self)  { ... }
  *
  * // 3. 定义子类虚函数表
  * static const MotorVTable dcmotor_vtable = {
  *     .init = DCMotor_init, .run = DCMotor_run, .cleanup = NULL, .reset = NULL,
  * };
  *
  * // 4. 构造并初始化
  * DCMotor motor;
  * Motor_Constructor(&motor.base, "Left_DCMotor");
  * motor.base.vtable = &dcmotor_vtable;
  * Motor_Init(&motor.base);
  *
  * // 5. 主循环中运行
  * Motor_Run(&motor.base);
  *
  ******************************************************************************
  */

#ifndef __MOTOR_BASE_H__
#define __MOTOR_BASE_H__

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

/* ==================== 电机运行状态枚举 ==================== */

typedef enum {
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_RUNNING = 1,
    MOTOR_STATE_ERROR   = 2
} MotorState;

/* ==================== 虚函数表定义 ==================== */

typedef struct {
    int (*init)(void *self);
    int (*run)(void *self);
    int (*cleanup)(void *self);
    void (*reset)(void *self);
} MotorVTable;

/* ==================== 电机基类结构体 ==================== */

typedef struct MotorBase_s {
    const MotorVTable *vtable;
    const char        *name;
    uint8_t            initialized;
    MotorState         state;
} MotorBase;

/* ==================== 公有接口函数 ==================== */

void        Motor_Constructor(MotorBase *self, const char *name);
int         Motor_Init(MotorBase *self);
int         Motor_Run(MotorBase *self);
int         Motor_Cleanup(MotorBase *self);
const char *Motor_GetName(const MotorBase *self);
uint8_t     Motor_IsInitialized(const MotorBase *self);

#endif /* __MOTOR_BASE_H__ */
