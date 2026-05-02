/**
  ******************************************************************************
  * @file    LineSensor.c
  * @brief   八路灰度循迹传感器子类实现 — 通道扫描与位置解算
  *
  * @details
  * 提供 LineSensor_t 子类的完整实现，包括：
  *   - 虚函数表（init / run / cleanup / reset）
  *   - 通道片选扫描（AD0-AD2 3 位编码 → 8 通道）
  *   - 黑线位置加权解算（mm）
  *   - 调试打印（共用 UART1，200ms 节流）
  *
  * === 扫描时序 ===
  *
  * 每次 run() 执行 8 通道扫描：
  *   1. 设置 AD0-AD2 选择通道 i
  *   2. 读取 OUT 引脚电平 → channel_values[i]
  *   3. 加权计算黑线位置
  *
  * 168MHz Cortex-M4 下单次扫描 < 20µs，200Hz (5ms) 周期完全满足。
  *
  * === 位置计算公式 ===
  *
  * 通道 i 中心相对传感器中心偏移: pos_i = (i - 3.5) × 11.5 mm
  * 加权位置: line_position = Σ(val[i] × pos_i) / Σ(val[i])
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "LineSensor.h"

/* ==================== 私有辅助函数 ==================== */

/**
  * @brief  选择并读取指定通道
  * @note   通过 AD0-AD2 输出 3 位二进制编码选择通道，再读取 OUT 引脚。
  *         AD0 = BIT0, AD1 = BIT1, AD2 = BIT2
  *         CH1=000, CH2=001, ..., CH8=111
  * @param  self    指向传感器对象的指针（调用方保证非空）
  * @param  channel 通道索引 (0~7)
  * @return uint8_t 1=黑线, 0=白底
  */
static uint8_t LineSensor_ReadChannel(LineSensor_t *self, uint8_t channel)
{
    /* 设置 AD0 (BIT0) */
    HAL_GPIO_WritePin(self->ad_port, self->ad0_pin,
                      (channel & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* 设置 AD1 (BIT1) */
    HAL_GPIO_WritePin(self->ad_port, self->ad1_pin,
                      (channel & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* 设置 AD2 (BIT2) */
    HAL_GPIO_WritePin(self->ad_port, self->ad2_pin,
                      (channel & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* 读取 OUT 引脚，高电平=黑线 */
    return (HAL_GPIO_ReadPin(self->out_port, self->out_pin) == GPIO_PIN_SET) ? 1 : 0;
}

/**
  * @brief  扫描全部 8 通道
  * @note   依次选择 CH1~CH8，读取 OUT 引脚，填入 channel_values[]。
  * @param  self  指向传感器对象的指针（调用方保证非空）
  */
static void LineSensor_ScanAll(LineSensor_t *self)
{
    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        self->channel_values[i] = LineSensor_ReadChannel(self, i);
    }
}

/**
  * @brief  计算黑线位置（加权平均）
  * @note   以传感器中心（CH4 和 CH5 之间）为原点，左负右正。
  *         通道 i 中心偏移 = (i - 3.5) × 通道间距(11.5mm)
  *         例如：CH1(i=0) → -40.25mm, CH8(i=7) → +40.25mm
  *
  *         更新 self->line_position, self->active_channels, self->line_detected
  * @param  self  指向传感器对象的指针（调用方保证非空）
  */
static void LineSensor_CalcPosition(LineSensor_t *self)
{
    float sum_weighted = 0.0f;
    uint8_t sum_weights = 0;

    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        if (self->channel_values[i]) {
            /* 通道 i 中心相对传感器中心的偏移 (mm) */
            float pos = ((float)i - 3.5f) * LS_CHANNEL_SPACING_MM;
            sum_weighted += pos;
            sum_weights++;
        }
    }

    self->active_channels = sum_weights;

    if (sum_weights > 0) {
        self->line_position  = sum_weighted / (float)sum_weights;
        self->line_detected  = 1;
    } else {
        self->line_position  = 0.0f;
        self->line_detected  = 0;
    }
}

/* ==================== 虚函数实现 ==================== */

/**
  * @brief  灰度传感器初始化虚函数
  * @note   配置 AD0-AD2 为推挽输出，OUT 为输入。
  *         清零所有数据字段。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int LineSensor_init(void *self)
{
    LineSensor_t *ls = (LineSensor_t *)self;
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. 配置 AD0-AD2 为推挽输出 */
    GPIO_InitStruct.Pin   = ls->ad0_pin | ls->ad1_pin | ls->ad2_pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ls->ad_port, &GPIO_InitStruct);

    /* 2. 配置 OUT 为输入 */
    GPIO_InitStruct.Pin  = ls->out_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ls->out_port, &GPIO_InitStruct);

    /* 3. 清零所有数据 */
    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        ls->channel_values[i] = 0;
    }
    ls->line_detected   = 0;
    ls->line_position   = 0.0f;
    ls->active_channels = 0;
    ls->last_print_tick = 0;

    return 0;
}

/**
  * @brief  灰度传感器运行虚函数 — 周期性扫描与位置解算
  * @note   按 update_period_ms 周期调用（默认 5ms = 200Hz）。
  *         执行顺序：扫描 8 通道 → 计算位置。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     成功
  */
static int LineSensor_run(void *self)
{
    LineSensor_t *ls = (LineSensor_t *)self;

    /* 扫描全部 8 通道 */
    LineSensor_ScanAll(ls);

    /* 加权计算黑线位置 */
    LineSensor_CalcPosition(ls);

    return 0;
}

/**
  * @brief  灰度传感器清理虚函数
  * @note   清零所有传感器数据。
  * @param  self  指向模块对象自身的 void 指针
  * @retval 0     始终返回成功
  */
static int LineSensor_cleanup(void *self)
{
    LineSensor_t *ls = (LineSensor_t *)self;

    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        ls->channel_values[i] = 0;
    }
    ls->line_detected   = 0;
    ls->line_position   = 0.0f;
    ls->active_channels = 0;
    ls->last_print_tick = 0;

    return 0;
}

/**
  * @brief  灰度传感器复位虚函数
  * @note   清零所有传感器数据。
  * @param  self  指向模块对象自身的 void 指针
  */
static void LineSensor_reset(void *self)
{
    LineSensor_t *ls = (LineSensor_t *)self;

    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        ls->channel_values[i] = 0;
    }
    ls->line_detected   = 0;
    ls->line_position   = 0.0f;
    ls->active_channels = 0;
    ls->last_print_tick = 0;
}

/* ==================== 子类虚函数表实例 ==================== */

/**
  * @brief 灰度传感器虚函数表
  * @note  所有虚函数均被重写，提供完整的传感器行为。
  */
static const SensorVTable_t linesensor_vtable = {
    .init    = LineSensor_init,
    .run     = LineSensor_run,
    .cleanup = LineSensor_cleanup,
    .reset   = LineSensor_reset,
};

/* ==================== 公有接口实现 ==================== */

/**
  * @brief  灰度传感器构造函数
  * @param  self       指向传感器对象的指针
  * @param  ad_port    AD0-AD2 所在 GPIO 端口
  * @param  ad0_pin    AD0 引脚号
  * @param  ad1_pin    AD1 引脚号
  * @param  ad2_pin    AD2 引脚号
  * @param  out_port   OUT 所在 GPIO 端口
  * @param  out_pin    OUT 引脚号
  */
void LineSensor_Constructor(LineSensor_t *self,
                            GPIO_TypeDef *ad_port,
                            uint16_t ad0_pin, uint16_t ad1_pin, uint16_t ad2_pin,
                            GPIO_TypeDef *out_port, uint16_t out_pin)
{
    if (self == NULL) {
        return;
    }

    /* 1. 调用基类构造函数 */
    SensorBase_Constructor(&self->base, "LineSensor");

    /* 2. 设置默认更新周期 (200Hz) */
    self->base.update_period_ms = LS_UPDATE_PERIOD_MS;

    /* 3. 初始化 GPIO 引脚 */
    self->ad_port  = ad_port;
    self->ad0_pin  = ad0_pin;
    self->ad1_pin  = ad1_pin;
    self->ad2_pin  = ad2_pin;
    self->out_port = out_port;
    self->out_pin  = out_pin;

    /* 4. 清零数据字段 */
    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        self->channel_values[i] = 0;
    }
    self->line_detected   = 0;
    self->line_position   = 0.0f;
    self->active_channels = 0;
    self->last_print_tick = 0;

    /* 5. 替换为子类虚函数表 */
    self->base.vtable = &linesensor_vtable;
}

/**
  * @brief  获取 8 路通道原始值（只读）
  * @param  self  指向传感器对象的指针
  * @return const uint8_t*  指向 8 字节 channel_values 数组的指针
  */
const uint8_t *LineSensor_GetChannelValues(const LineSensor_t *self)
{
    if (self == NULL) {
        return NULL;
    }
    return self->channel_values;
}

/**
  * @brief  获取黑线位置
  * @param  self  指向传感器对象的指针
  * @return float 黑线位置 (mm)，传感器中心为 0，左负右正
  */
float LineSensor_GetPosition(const LineSensor_t *self)
{
    if (self == NULL) {
        return 0.0f;
    }
    return self->line_position;
}

/**
  * @brief  获取检测到黑线的通道数
  * @param  self  指向传感器对象的指针
  * @return uint8_t  活动通道数 (0~8)
  */
uint8_t LineSensor_GetActiveChannels(const LineSensor_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->active_channels;
}

/**
  * @brief  查询是否检测到黑线
  * @param  self  指向传感器对象的指针
  * @return uint8_t  0 = 无黑线, 1 = 检测到黑线
  */
uint8_t LineSensor_IsLineDetected(const LineSensor_t *self)
{
    if (self == NULL) {
        return 0;
    }
    return self->line_detected;
}

/**
  * @brief  打印灰度传感器信息（通过 DebugPrintf DMA 发送）
  * @note   内置 200ms 速率限制（per-instance），避免刷屏。
  *         单行格式示例：
  *         "[LineSensor] CH:01001100 Pos:+11.5mm Act:3"
  *         self 或 dbg 为 NULL 时静默返回。
  * @param  self  指向传感器对象的指针
  * @param  dbg   指向 DebugPrintf 对象的指针
  */
void LineSensor_PrintInfo(LineSensor_t *self, DebugPrintf_t *dbg)
{
    if (self == NULL || dbg == NULL) {
        return;
    }

    uint32_t now = HAL_GetTick();

    /* 200ms 速率限制 */
    if (now - self->last_print_tick < LS_PRINT_INTERVAL_MS) {
        return;
    }
    self->last_print_tick = now;

    /* 构建 8 位二进制字符串，如 "01001100" (CH1 在左/MSB) */
    char ch_str[LS_CHANNEL_COUNT + 1];
    for (uint8_t i = 0; i < LS_CHANNEL_COUNT; i++) {
        ch_str[i] = self->channel_values[i] ? '1' : '0';
    }
    ch_str[LS_CHANNEL_COUNT] = '\0';

    /* 输出: "[LineSensor] CH:01001100 Pos:+11.5mm Act:3" */
    DebugPrintf_Print(dbg, "[LineSensor] CH:%s Pos:%+.1fmm Act:%u\r\n",
                      ch_str,
                      (double)self->line_position,
                      self->active_channels);
}
