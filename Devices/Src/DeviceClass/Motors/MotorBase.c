/**
  ******************************************************************************
  * @file    MotorBase.c
  * @brief   电机模块基类实现 — 默认虚函数 + 构造 + 公有接口
  *
  * @details
  * 提供 MotorBase 的默认虚函数实现和默认虚函数表实例。
  * 子类（DCMotor、StepMotor）通过替换虚函数表实现多态。
  *
  * === 设计说明 ===
  *
  * 1. 默认虚函数表 (default_vtable)
  *    所有函数指针指向空实现。子类必须替换以实现实际行为。
  *
  * 2. 初始化状态跟踪
  *    initialized 标志由 Motor_Init（置 1）和 Motor_Cleanup（清 0）管理。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "MotorBase.h"

/* ==================== 默认虚函数实现 ==================== */

static int Motor_defaultInit(void *self)
{
    (void)self;
    return 0;
}

static int Motor_defaultRun(void *self)
{
    (void)self;
    return 0;
}

static int Motor_defaultCleanup(void *self)
{
    (void)self;
    return 0;
}

static void Motor_defaultReset(void *self)
{
    (void)self;
}

/* ==================== 默认虚函数表实例 ==================== */

static const MotorVTable default_vtable = {
    .init    = Motor_defaultInit,
    .run     = Motor_defaultRun,
    .cleanup = Motor_defaultCleanup,
    .reset   = Motor_defaultReset,
};

/* ==================== 公有接口实现 ==================== */

void Motor_Constructor(MotorBase *self, const char *name)
{
    if (self == NULL) {
        return;
    }

    self->vtable      = &default_vtable;
    self->name        = name;
    self->initialized = 0;
    self->state       = MOTOR_STATE_STOPPED;
}

int Motor_Init(MotorBase *self)
{
    int ret;

    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    if (self->initialized) {
        return 0;
    }

    if (self->vtable->init != NULL) {
        ret = self->vtable->init((void *)self);
    } else {
        ret = 0;
    }

    if (ret == 0) {
        self->initialized = 1;
    }

    return ret;
}

int Motor_Run(MotorBase *self)
{
    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    if (!self->initialized) {
        return -1;
    }

    if (self->vtable->run != NULL) {
        return self->vtable->run((void *)self);
    }

    return 0;
}

int Motor_Cleanup(MotorBase *self)
{
    int ret = 0;

    if ((self == NULL) || (self->vtable == NULL)) {
        return -1;
    }

    if (!self->initialized) {
        return 0;
    }

    if (self->vtable->cleanup != NULL) {
        ret = self->vtable->cleanup((void *)self);
    }

    self->initialized = 0;

    return ret;
}

const char *Motor_GetName(const MotorBase *self)
{
    if (self == NULL) {
        return "NULL";
    }
    return (self->name != NULL) ? self->name : "UNKNOWN";
}

uint8_t Motor_IsInitialized(const MotorBase *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->initialized;
}
