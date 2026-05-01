/**
  ******************************************************************************
  * @file    Encoder.c
  * @brief   编码器传感器子类实现 — 霍尔编码器脉冲计数与运动解算
  *
  * @details
  * 提供 Encoder_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - 脉冲计数与 16 位计数器回绕处理
  *   - 转速（RPM）、线速度（mm/s）、距离（mm）计算
  *   - 数据清零与硬件复位
  *
  * === 计算公式 ===
  *
  * RPM  = pulse_diff * 60 / update_period_s / PULSE_PER_ROUND
  * MMPS = RPM * WHEEL_CIRCUMFERENCE / 60
  * Distance_MM += MMPS * update_period_s
  *
  * 其中 PULSE_PER_ROUND = ENCODER_LINE * 4 * GEAR_RATIO
  * （4 倍频来自 TIM 编码器模式 TI1+TI2 双边沿计数）
  *
  * === 16 位计数器回绕处理 ===
  *
  * TIM 编码器计数器为 16 位（0~65535），通过将当前值强制转换为 int16_t
  * 后与上次值做差，利用有符号整数运算自动处理回绕（65535→0 变为 -1）。
  * 此方法要求更新周期足够短，保证单周期内脉冲变化不超过 ±32767。
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "Encoder.h"
#include <stdio.h>

/* ==================== 私有宏定义 ==================== */

/**
  * @defgroup Encoder_Private_Macros 私有宏
  * @{
  */
#define ENCODER_UPDATE_PERIOD_MS    40U     /**< 默认更新周期 (ms)，即 25Hz */
#define ENCODER_PRINT_INTERVAL_MS   500U    /**< 打印速率限制间隔 (ms) */
#define SECONDS_PER_MINUTE          60.0f   /**< 每分钟秒数 */
#define MS_TO_SEC(ms)               ((float)(ms) / 1000.0f)  /**< 毫秒转秒 */
/**
  * @}
  */

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  读取 TIM 计数器并计算脉冲差
  * @note   利用 int16_t 强制转换自动处理 16 位计数器回绕。
  *         更新周期 40ms，最大可检测脉冲变化 ±32767，
  *         对应转速约 ±1852 RPM，远大于电机额定 460 RPM。
  * @param  enc  指向编码器对象的指针（调用方保证非空）
  */
static void Encoder_GetPulse(Encoder_t *enc)
{
    /* 1. 读取当前 TIM 计数器值，强制转换为 int16_t
     *    原理：丢弃高 16 位，将低 16 位解释为有符号数
     *    使 65535→0 的回绕自然变为 -1 而非跳变 */
    enc->current_cnt = (int16_t)__HAL_TIM_GetCounter(enc->tim_handle);

    /* 2. 计算差值（有符号，自动处理回绕）*/
    enc->pulse_diff = (int32_t)(enc->current_cnt - enc->last_cnt);

    /* 3. 更新上次计数器值 */
    enc->last_cnt = enc->current_cnt;

    /* 4. 累加到总脉冲数 */
    enc->total_pulse += enc->pulse_diff;
}

/**
  * @brief  更新运动参数（RPM / MMPS / Distance）
  * @note   基于当前 pulse_diff 和 update_period_ms 计算。
  *         调用前必须已执行 Encoder_GetPulse()。
  * @param  enc  指向编码器对象的指针（调用方保证非空）
  */
static void Encoder_UpdateKinematics(Encoder_t *enc)
{
    float period_s;  /* 更新周期（秒）*/

    /* 获取更新周期，若基类未设置则使用默认值 */
    if (enc->base.update_period_ms > 0) {
        period_s = MS_TO_SEC(enc->base.update_period_ms);
    } else {
        period_s = MS_TO_SEC(ENCODER_UPDATE_PERIOD_MS);
    }

    /* RPM = 脉冲差 / 单圈脉冲数 / 周期(s) * 60(s/min) */
    enc->rpm = (float)enc->pulse_diff * SECONDS_PER_MINUTE
             / period_s / PULSE_PER_ROUND;

    /* MMPS = RPM * 车轮周长(mm) / 60(s/min) */
    enc->mmps = enc->rpm * WHEEL_CIRCUMFERENCE / SECONDS_PER_MINUTE;

    /* 累积距离 */
    enc->distance_mm += enc->mmps * period_s;
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  编码器初始化虚函数
  * @note   启动 TIM 编码器模式，清零所有数据字段。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -1    TIM 句柄无效
  */
static int Encoder_init(void *self)
{
    Encoder_t *enc = (Encoder_t *)self;

    /* 参数完整性检查 */
    if (enc->tim_handle == NULL) {
        return -1;
    }

    /* 启动 TIM 编码器模式（双通道 TI1+TI2）*/
    if (HAL_TIM_Encoder_Start(enc->tim_handle, TIM_CHANNEL_ALL) != HAL_OK) {
        return -2;
    }

    /* 清零所有数据和计数器 */
    __HAL_TIM_SetCounter(enc->tim_handle, 0);
    enc->current_cnt     = 0;
    enc->last_cnt        = 0;
    enc->pulse_diff      = 0;
    enc->total_pulse     = 0;
    enc->rpm             = 0.0f;
    enc->mmps            = 0.0f;
    enc->distance_mm     = 0.0f;
    enc->last_print_tick = 0;

    return 0;
}

/**
  * @brief  编码器运行虚函数 — 周期性数据更新
  * @note   在定时中断或主循环中按 update_period_ms 周期调用。
  *         执行顺序：读取脉冲 → 计算运动参数。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  * @retval -2    TIM 句柄无效
  */
static int Encoder_run(void *self)
{
    Encoder_t *enc = (Encoder_t *)self;

    if (enc->tim_handle == NULL) {
        return -2;
    }

    /* 读取脉冲并计算差值 */
    Encoder_GetPulse(enc);

    /* 更新运动学参数 */
    Encoder_UpdateKinematics(enc);

    return 0;
}

/**
  * @brief  编码器清理虚函数
  * @note   停止 TIM 编码器模式，清零所有数据。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int Encoder_cleanup(void *self)
{
    Encoder_t *enc = (Encoder_t *)self;

    /* 停止 TIM 编码器模式 */
    if (enc->tim_handle != NULL) {
        HAL_TIM_Encoder_Stop(enc->tim_handle, TIM_CHANNEL_ALL);
    }

    /* 清零所有数据 */
    enc->current_cnt     = 0;
    enc->last_cnt        = 0;
    enc->pulse_diff      = 0;
    enc->total_pulse     = 0;
    enc->rpm             = 0.0f;
    enc->mmps            = 0.0f;
    enc->distance_mm     = 0.0f;
    enc->last_print_tick = 0;

    return 0;
}

/**
  * @brief  编码器复位虚函数 — 完全硬件+数据复位
  * @note   重置 TIM 计数器并清零所有软件累积数据。
  * @param  self  指向模块对象自身的 void 指针
  */
static void Encoder_reset(void *self)
{
    Encoder_t *enc = (Encoder_t *)self;

    /* 重置 TIM 硬件计数器 */
    if (enc->tim_handle != NULL) {
        __HAL_TIM_SetCounter(enc->tim_handle, 0);
    }

    /* 清零所有软件数据 */
    enc->current_cnt     = 0;
    enc->last_cnt        = 0;
    enc->pulse_diff      = 0;
    enc->total_pulse     = 0;
    enc->rpm             = 0.0f;
    enc->mmps            = 0.0f;
    enc->distance_mm     = 0.0f;
    enc->last_print_tick = 0;
}

/* ==================== 子类虚函数表实例 ==================== */

/**
  * @brief 编码器模块虚函数表
  * @note  所有虚函数均被重写，提供完整的编码器行为。
  */
static const SensorVTable_t encoder_vtable = {
    .init    = Encoder_init,
    .run     = Encoder_run,
    .cleanup = Encoder_cleanup,
    .reset   = Encoder_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  编码器构造函数
  * @param  self         指向编码器对象的指针
  * @param  tim_handle   编码器模式 TIM 句柄指针
  * @param  motor_index  电机索引（0=左电机, 1=右电机）
  */
void Encoder_Constructor(Encoder_t *self, TIM_HandleTypeDef *tim_handle, uint8_t motor_index)
{
    if (self == NULL) {
        return;
    }

    /* 1. 调用基类构造函数 */
    SensorBase_Constructor(&self->base, "Encoder");

    /* 2. 设置默认更新周期 */
    self->base.update_period_ms = ENCODER_UPDATE_PERIOD_MS;

    /* 3. 初始化编码器特有成员 */
    self->tim_handle      = tim_handle;
    self->motor_index     = motor_index;
    self->current_cnt     = 0;
    self->last_cnt        = 0;
    self->pulse_diff      = 0;
    self->total_pulse     = 0;
    self->rpm             = 0.0f;
    self->mmps            = 0.0f;
    self->distance_mm     = 0.0f;
    self->last_print_tick = 0;

    /* 4. 替换为子类虚函数表 */
    self->base.vtable = &encoder_vtable;
}

/**
  * @brief  获取当前转速
  * @param  self  指向编码器对象的指针
  * @return float 转速 (RPM)
  */
float Encoder_GetRPM(const Encoder_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->rpm;
}

/**
  * @brief  获取当前线速度
  * @param  self  指向编码器对象的指针
  * @return float 线速度 (mm/s)
  */
float Encoder_GetMMPS(const Encoder_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->mmps;
}

/**
  * @brief  获取累积行驶距离
  * @param  self  指向编码器对象的指针
  * @return float 累积距离 (mm)
  */
float Encoder_GetDistance(const Encoder_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->distance_mm;
}

/**
  * @brief  获取累计脉冲数
  * @param  self  指向编码器对象的指针
  * @return int64_t 累计脉冲数
  */
int64_t Encoder_GetTotalPulse(const Encoder_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->total_pulse;
}

/**
  * @brief  获取本次脉冲差
  * @param  self  指向编码器对象的指针
  * @return int32_t 本轮脉冲差（带符号）
  */
int32_t Encoder_GetPulseDiff(const Encoder_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->pulse_diff;
}

/**
  * @brief  清零所有累积数据（保留硬件计数器运行）
  * @note   用于多段运动场景，仅清零软件累积值。
  *         当前帧的 last_cnt 同步为 current_cnt，避免下一帧产生虚假差值。
  * @param  self  指向编码器对象的指针
  */
void Encoder_ClearData(Encoder_t *self)
{
    if (self == NULL) {
        return;
    }

    /* 同步 last_cnt，避免清零后下一帧产生虚假跳变 */
    self->last_cnt        = self->current_cnt;
    self->pulse_diff      = 0;
    self->total_pulse     = 0;
    self->rpm             = 0.0f;
    self->mmps            = 0.0f;
    self->distance_mm     = 0.0f;
    self->last_print_tick = 0;
}

/**
  * @brief  完全复位编码器数据（含硬件计数器）
  * @note   同时清零软件数据和 TIM 硬件计数器。
  * @param  self  指向编码器对象的指针
  */
void Encoder_HardReset(Encoder_t *self)
{
    if (self == NULL) {
        return;
    }

    /* 重置 TIM 硬件计数器 */
    if (self->tim_handle != NULL) {
        __HAL_TIM_SetCounter(self->tim_handle, 0);
    }

    /* 清零所有软件数据 */
    self->current_cnt     = 0;
    self->last_cnt        = 0;
    self->pulse_diff      = 0;
    self->total_pulse     = 0;
    self->rpm             = 0.0f;
    self->mmps            = 0.0f;
    self->distance_mm     = 0.0f;
    self->last_print_tick = 0;
}

/* ==================== 调试打印接口 ==================== */

/**
  * @brief  打印编码器运动信息（通过 DebugPrintf DMA 发送）
  * @note   内置 0.5s 速率限制（per-instance），避免刷屏。
  *         若距离上次成功打印不足 500ms，则静默跳过。
  *         self 或 dbg 为 NULL 时静默返回。
  * @param  self    指向编码器对象的指针
  * @param  dbg     指向 DebugPrintf 对象的指针
  * @param  label   标签字符（如 'L'=左, 'R'=右）
  * @param  is_last 是否为最后一行（0=末尾加 \r\n, 1=不加）
  */
void Encoder_PrintInfo(Encoder_t *self, DebugPrintf_t *dbg, char label, uint8_t is_last)
{
    if (self == NULL || dbg == NULL) {
        return;
    }

    uint32_t now = HAL_GetTick();

    /* 0.5s 速率限制 */
    if (now - self->last_print_tick < ENCODER_PRINT_INTERVAL_MS) {
        return;
    }
    self->last_print_tick = now;

    /* 格式化并发送，格式: "[L] RPM=123.4 Speed=456.7mm/s Dist=100.0mm Pulse=500 Diff=10" */
    DebugPrintf_Print(dbg, "[%c] RPM=%.1f Speed=%.1fmm/s Dist=%.1fmm Pulse=%lld Diff=%ld%s",
                      label,
                      (double)self->rpm,
                      (double)self->mmps,
                      (double)self->distance_mm,
                      (long long)self->total_pulse,
                      (long)self->pulse_diff,
                      is_last ? "" : "\r\n");
}

/**
  * @brief  打印双编码器运动信息（便捷函数）
  * @note   内置 0.5s 速率限制（基于左编码器的 last_print_tick）。
  *         输出 3 行：header + left data + right data，最后一行无尾随 \r\n。
  *         任一参数为 NULL 时静默返回。
  * @param  left  左编码器指针
  * @param  right 右编码器指针
  * @param  dbg   DebugPrintf 对象指针
  */
void Encoder_PrintDualInfo(Encoder_t *left, Encoder_t *right, DebugPrintf_t *dbg)
{
    if (left == NULL || right == NULL || dbg == NULL) {
        return;
    }

    uint32_t now = HAL_GetTick();

    /* 0.5s 速率限制（基于左编码器） */
    if (now - left->last_print_tick < ENCODER_PRINT_INTERVAL_MS) {
        return;
    }
    left->last_print_tick = now;

    /* 第1行: 标题头（时间戳） */
    DebugPrintf_Print(dbg, "=== Encoder t=%lu.%03lus ===\r\n",
                      (unsigned long)(now / 1000),
                      (unsigned long)(now % 1000));

    /* 左编码器行 */
    DebugPrintf_Print(dbg, "[L] RPM=%.1f Speed=%.1fmm/s Dist=%.1fmm Pulse=%lld Diff=%ld\r\n",
                      (double)left->rpm,
                      (double)left->mmps,
                      (double)left->distance_mm,
                      (long long)left->total_pulse,
                      (long)left->pulse_diff);

    /* 最后一行：无尾随 \r\n */
    DebugPrintf_Print(dbg, "[R] RPM=%.1f Speed=%.1fmm/s Dist=%.1fmm Pulse=%lld Diff=%ld",
                      (double)right->rpm,
                      (double)right->mmps,
                      (double)right->distance_mm,
                      (long long)right->total_pulse,
                      (long)right->pulse_diff);
}
