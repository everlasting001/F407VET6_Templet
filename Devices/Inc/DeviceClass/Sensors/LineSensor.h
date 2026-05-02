/**
  ******************************************************************************
  * @file    LineSensor.h
  * @brief   八路灰度循迹传感器子类 — 继承 SensorBase 的巡线传感器
  *
  * @details
  * 本文件定义了八路灰度循迹传感器模块的结构体和公有接口，继承自 SensorBase 基类。
  * 基类提供 name、initialized、update_period_ms 等属性，子类专注实现：
  *   - 通道片选扫描（AD0-AD2 3 位二进制编码选择 8 通道）
  *   - OUT 引脚读取检测结果（高电平=黑线）
  *   - 黑线位置加权解算（mm）
  *   - 调试打印（共用 UART1，与 Vofa 同通道）
  *
  * === 硬件配置（来自 Config.md）===
  *
  * 通道间距:     11.5mm
  * 通道选择引脚:  AD0=PD12, AD1=PD13, AD2=PD14 (推挽输出)
  * 检测结果引脚:  OUT=PD11 (输入，高电平=黑线)
  * 片选编码:      CH1=000 → CH8=111 (AD2 AD1 AD0)
  *
  * === 使用示例 ===
  *
  * // 1. 构造
  * LineSensor_t line;
  * LineSensor_Constructor(&line, GPIOD,
  *                        GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14,
  *                        GPIOD, GPIO_PIN_11);
  *
  * // 2. 初始化
  * SensorBase_Init((SensorBase_t *)&line);
  *
  * // 3. 周期性更新（200Hz，在 TIM2 ISR 中软件分频或主循环）
  * SensorBase_Run((SensorBase_t *)&line);
  *
  * // 4. 读取数据
  * const uint8_t *vals = LineSensor_GetChannelValues(&line);
  * float pos = LineSensor_GetPosition(&line);
  *
  * // 5. 调试打印（通过 DebugPrintf，与 Vofa 共用 UART1）
  * LineSensor_PrintInfo(&line, &dbg_printf);
  *
  ******************************************************************************
  */

#ifndef __LINE_SENSOR_H__
#define __LINE_SENSOR_H__

/* Includes ------------------------------------------------------------------*/
#include "SensorBase.h"
#include "stm32f4xx_hal.h"
#include "gpio.h"
#include "DebugPrintf.h"

/* ==================== 传感器配置宏 ==================== */

/**
  * @defgroup LineSensor_Config 灰度传感器硬件配置
  * @{
  */
#define LS_CHANNEL_COUNT        8               /**< 通道数量 */
#define LS_CHANNEL_SPACING_MM   11.5f           /**< 通道间距 (mm) */
#define LS_UPDATE_PERIOD_MS     5U              /**< 默认更新周期 (ms)，即 200Hz */
#define LS_PRINT_INTERVAL_MS    200U            /**< 打印速率限制 (ms)，即 5Hz */
/**
  * @}
  */

/* ==================== 传感器结构体定义 ==================== */

/**
  * @brief 八路灰度循迹传感器结构体（继承 SensorBase）
  *
  * @note   SensorBase_t 必须为第一个成员，确保指针可安全转换
  */
typedef struct LineSensor_s {
    SensorBase_t       base;             /**< 基类（必须为第一个成员）*/

    /* GPIO 引脚 */
    GPIO_TypeDef      *ad_port;          /**< AD0-AD2 所在 GPIO 端口 (GPIOD) */
    uint16_t           ad0_pin;          /**< AD0 引脚 (PD12) — 通道选择 BIT0 */
    uint16_t           ad1_pin;          /**< AD1 引脚 (PD13) — 通道选择 BIT1 */
    uint16_t           ad2_pin;          /**< AD2 引脚 (PD14) — 通道选择 BIT2 */
    GPIO_TypeDef      *out_port;         /**< OUT 所在 GPIO 端口 (GPIOD) */
    uint16_t           out_pin;          /**< OUT 引脚 (PD11) — 检测结果输入 */

    /* 传感器数据 */
    uint8_t            channel_values[LS_CHANNEL_COUNT]; /**< 8 路原始值 (0=白/1=黑) */
    uint8_t            line_detected;     /**< 是否检测到黑线 (0=无, 1=有) */
    float              line_position;     /**< 黑线位置 (mm)，传感器中心为 0，左负右正 */
    uint8_t            active_channels;   /**< 检测到黑线的通道数 */

    /* 打印速率限制 */
    uint32_t           last_print_tick;   /**< 上次打印时刻 (HAL_GetTick)，200ms 节流 */
} LineSensor_t;

/* ==================== 公有接口函数 ==================== */

/**
  * @brief  灰度传感器构造函数
  * @note   初始化基类成员，设置 GPIO 引脚。构造后默认清零所有数据字段。
  * @param  self       指向传感器对象的指针
  * @param  ad_port    AD0-AD2 所在 GPIO 端口（通常为 GPIOD）
  * @param  ad0_pin    AD0 引脚号（如 GPIO_PIN_12）
  * @param  ad1_pin    AD1 引脚号（如 GPIO_PIN_13）
  * @param  ad2_pin    AD2 引脚号（如 GPIO_PIN_14）
  * @param  out_port   OUT 所在 GPIO 端口（通常为 GPIOD）
  * @param  out_pin    OUT 引脚号（如 GPIO_PIN_11）
  */
void LineSensor_Constructor(LineSensor_t *self,
                            GPIO_TypeDef *ad_port,
                            uint16_t ad0_pin, uint16_t ad1_pin, uint16_t ad2_pin,
                            GPIO_TypeDef *out_port, uint16_t out_pin);

/**
  * @brief  获取 8 路通道原始值（只读）
  * @param  self  指向传感器对象的指针
  * @return const uint8_t*  指向 8 字节 channel_values 数组的指针，参数无效时返回 NULL
  */
const uint8_t *LineSensor_GetChannelValues(const LineSensor_t *self);

/**
  * @brief  获取黑线位置
  * @param  self  指向传感器对象的指针
  * @return float 黑线位置 (mm)，传感器中心为 0，左负右正。参数无效或无黑线时返回 0.0f
  */
float LineSensor_GetPosition(const LineSensor_t *self);

/**
  * @brief  获取检测到黑线的通道数
  * @param  self  指向传感器对象的指针
  * @return uint8_t  活动通道数 (0~8)，参数无效时返回 0
  */
uint8_t LineSensor_GetActiveChannels(const LineSensor_t *self);

/**
  * @brief  查询是否检测到黑线
  * @param  self  指向传感器对象的指针
  * @return uint8_t  0 = 无黑线, 1 = 检测到黑线
  */
uint8_t LineSensor_IsLineDetected(const LineSensor_t *self);

/**
  * @brief  打印灰度传感器信息（通过 DebugPrintf DMA 发送）
  * @note   内置 200ms 速率限制（per-instance），避免刷屏。
  *         单行格式示例：
  *         "[LineSensor] CH:01001100 Pos:+11.5mm Act:3"
  * @param  self  指向传感器对象的指针（NULL 安全）
  * @param  dbg   指向 DebugPrintf 对象的指针（NULL 安全，NULL 时不输出）
  */
void LineSensor_PrintInfo(LineSensor_t *self, DebugPrintf_t *dbg);

#endif /* __LINE_SENSOR_H__ */
