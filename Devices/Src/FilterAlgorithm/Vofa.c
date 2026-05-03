/**
  ******************************************************************************
  * @file    Vofa.c
  * @brief   Vofa+ FireWater 协议调参模块实现
  *
  * @details
  * 共用 USART1 (dbg_printf.uart) 与 Vofa+ 上位机通信。
  *
  * === 上行 13 通道数据帧 (MCU→Vofa, 10Hz) ===
  *   Vofa:pos_Kp,pos_Ki,pos_Kd,vel_Kp,vel_Ki,vel_Kd,
  *        balance_Kp,balance_Kd,vel_l_rpm,vel_r_rpm,avg_dist,pos_error,target\r\n
  *
  * === 下行 9 命令 (Vofa→MCU, Key:Value) ===
  *   pos_Kp, pos_Ki, pos_Kd, vel_Kp, vel_Ki, vel_Kd, balance_Kp, balance_Kd, Target
  ******************************************************************************
  */

#include "Vofa.h"
#include "Encoder.h"
#include "Gyro.h"
#include "LineSensor.h"
#include "SensorBase.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define VOFA_TX_BUF_SIZE  256

static UartBase_t    *vofa_uart;
static MoveControl_t *vofa_ctrl;
static Gyro_t        *vofa_gyro;

/* ==================== 下行解析 ==================== */

/**
  * @brief  解析 Vofa 下行命令 (格式: Key:Value\r\n)
  * @note   在 ISR 上下文中调用，仅做 strchr/strcmp/atof，快速返回。
  */
static void Vofa_ParseCommand(char *cmd_str)
{
    char *separator = strchr(cmd_str, ':');
    if (separator == NULL) return;

    *separator = '\0';
    char *key       = cmd_str;
    char *value_str = separator + 1;

    /* 去除末尾 \r \n */
    char *pos;
    if ((pos = strchr(value_str, '\r')) != NULL) *pos = '\0';
    if ((pos = strchr(value_str, '\n')) != NULL) *pos = '\0';

    float value = atof(value_str);

    if (vofa_ctrl == NULL) return;

    if (strcmp(key, "pos_Kp") == 0) {
        vofa_ctrl->pos_pid.kp = value;
    } else if (strcmp(key, "pos_Ki") == 0) {
        vofa_ctrl->pos_pid.ki = value;
    } else if (strcmp(key, "pos_Kd") == 0) {
        vofa_ctrl->pos_pid.kd = value;
    } else if (strcmp(key, "vel_Kp") == 0) {
        vofa_ctrl->vel_l_pid.kp = value;
        vofa_ctrl->vel_r_pid.kp = value;
    } else if (strcmp(key, "vel_Ki") == 0) {
        vofa_ctrl->vel_l_pid.ki = value;
        vofa_ctrl->vel_r_pid.ki = value;
    } else if (strcmp(key, "vel_Kd") == 0) {
        vofa_ctrl->vel_l_pid.kd = value;
        vofa_ctrl->vel_r_pid.kd = value;
    } else if (strcmp(key, "balance_Kp") == 0) {
        vofa_ctrl->balance_kp = value;
    } else if (strcmp(key, "balance_Kd") == 0) {
        vofa_ctrl->balance_kd = value;
    } else if (strcmp(key, "Target") == 0) {
        MoveControl_SetTarget(vofa_ctrl, value);
    } else if (strcmp(key, "base_pwm") == 0) {
        MoveControl_SetBasePWM(vofa_ctrl, value);
    } else if (strcmp(key, "k_line") == 0) {
        MoveControl_SetKLine(vofa_ctrl, value);
    } else if (strcmp(key, "kp_line") == 0) {
        MoveControl_SetKpLine(vofa_ctrl, value);
    } else if (strcmp(key, "Gyro_Reset") == 0) {
        if (vofa_gyro != NULL && value > 0.5f) {
            SensorBase_Reset((SensorBase_t *)vofa_gyro);
        }
    } else if (strcmp(key, "Gyro_ZeroDrift") == 0) {
        if (vofa_gyro != NULL && value > 0.0f) {
            vofa_gyro->zero_drift_threshold = value;
        }
    }
}

/* ==================== 公有接口 ==================== */

void Vofa_Init(UartBase_t *uart, MoveControl_t *ctrl)
{
    vofa_uart = uart;
    vofa_ctrl = ctrl;
    vofa_gyro = NULL;
}

/**
  * @brief  绑定陀螺仪实例 (供遥测和下行命令使用)
  * @param  gyro  指向 Gyro_t 实例的指针，传 NULL 则解绑
  */
void Vofa_SetGyro(Gyro_t *gyro)
{
    vofa_gyro = gyro;
}

/**
  * @brief  发送遥测数据帧 (FireWater CSV 格式, 非阻塞 DMA)
  * @note   调用频率: 10Hz (每 100ms) 或 20Hz (每 50ms)。
  *         若上次 TX 未完成则静默丢弃本帧。
  */
void Vofa_SendTelemetry(void)
{
    if (vofa_ctrl == NULL || vofa_uart == NULL) return;

    float vel_l = 0.0f;
    float vel_r = 0.0f;

    if (vofa_ctrl->encoder_left) {
        vel_l = Encoder_GetRPM(vofa_ctrl->encoder_left);
    }
    if (vofa_ctrl->encoder_right) {
        vel_r = Encoder_GetRPM(vofa_ctrl->encoder_right);
    }

    float avg_dist  = MoveControl_GetAvgDistance(vofa_ctrl);
    float pos_error = MoveControl_GetPositionError(vofa_ctrl);

    float pos_kp = vofa_ctrl->pos_pid.kp;
    float pos_ki = vofa_ctrl->pos_pid.ki;
    float pos_kd = vofa_ctrl->pos_pid.kd;
    float vel_kp = vofa_ctrl->vel_l_pid.kp;
    float vel_ki = vofa_ctrl->vel_l_pid.ki;
    float vel_kd = vofa_ctrl->vel_l_pid.kd;
    float bal_kp = vofa_ctrl->balance_kp;
    float bal_kd = vofa_ctrl->balance_kd;
    float target = vofa_ctrl->target_mm;

    char tx_buf[VOFA_TX_BUF_SIZE];
    int len = snprintf(tx_buf, sizeof(tx_buf),
        "Vofa:%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f\r\n",
        (double)pos_kp, (double)pos_ki, (double)pos_kd,
        (double)vel_kp, (double)vel_ki, (double)vel_kd,
        (double)bal_kp, (double)bal_kd,
        (double)vel_l, (double)vel_r,
        (double)avg_dist, (double)pos_error, (double)target);

    if (len > 0 && len < (int)sizeof(tx_buf)) {
        UartBase_SendStr(vofa_uart, tx_buf);
    }
}

/**
  * @brief  Vofa 中断回调 — 检查接收数据中是否含下行命令
  * @note   在 HAL_UARTEx_RxEventCallback (ISR 上下文) 中调用。
  *         复用 UartBase 的 rx_buffer，不启动自己的 DMA。
  *         仅快速解析 Key:Value 字符串，不调用阻塞 API。
  */
void Vofa_IRQHandler(UartBase_t *uart, uint16_t size)
{
    if (uart == NULL || uart->rx_buffer == NULL) return;
    if (size == 0 || size >= uart->rx_buf_size) return;

    uint8_t *data = uart->rx_buffer;
    uint16_t len  = size;

    /* 快速过滤: 无冒号则非 Vofa 下行命令，跳过 */
    uint8_t has_colon = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] == ':') {
            has_colon = 1;
            break;
        }
    }
    if (!has_colon) return;

    /* 复制到栈缓冲区解析 (保持 ISR 快速，避免原地修改 rx_buffer) */
    char buf[128];
    uint16_t copy_len = (len < sizeof(buf) - 1) ? len : (uint16_t)(sizeof(buf) - 1);
    memcpy(buf, (char *)data, copy_len);
    buf[copy_len] = '\0';

    Vofa_ParseCommand(buf);
}

/**
  * @brief  发送巡线遥测数据帧 (FireWater CSV 格式, 非阻塞 DMA)
  * @note   调用频率: 10Hz (每 100ms)。
  *         8 通道: pos_mm, kp_line, line_turn, base_pwm, left_pwm, right_pwm, ch_bits
  *         方案 B: line_turn = pos_mm × kp_line，PV 和增益分离显示
  */
void Vofa_SendLineTrackTelemetry(void)
{
    if (vofa_ctrl == NULL || vofa_uart == NULL) return;

    float pos_mm = 0.0f;
    if (vofa_ctrl->line_sensor) {
        pos_mm = LineSensor_GetPosition(vofa_ctrl->line_sensor);
    }
    float kp  = vofa_ctrl->kp_line;
    float lt  = vofa_ctrl->line_turn;
    float bp  = vofa_ctrl->base_pwm;
    float lp  = vofa_ctrl->line_left_pwm;
    float rp  = vofa_ctrl->line_right_pwm;
    uint8_t cb = vofa_ctrl->line_ch_bits;

    char tx_buf[VOFA_TX_BUF_SIZE];
    int len = snprintf(tx_buf, sizeof(tx_buf),
        "Line:%.1f,%.2f,%.1f,%.1f,%.1f,%.1f,%u\r\n",
        (double)pos_mm, (double)kp, (double)lt,
        (double)bp, (double)lp, (double)rp, cb);

    if (len > 0 && len < (int)sizeof(tx_buf)) {
        UartBase_SendStr(vofa_uart, tx_buf);
    }
}

/**
  * @brief  发送陀螺仪遥测数据帧 (FireWater CSV 格式, 非阻塞 DMA)
  * @note   调用频率: 10Hz (每 100ms)。
  *         3 通道: yaw, roll, pitch (欧拉角 °)
  *         使用前缀 "Gyro:" 与主遥测帧区分。
  */
void Vofa_SendGyroTelemetry(void)
{
    if (vofa_gyro == NULL || vofa_uart == NULL) return;

    float yaw   = Gyro_GetYaw(vofa_gyro);
    float roll  = Gyro_GetRoll(vofa_gyro);
    float pitch = Gyro_GetPitch(vofa_gyro);

    char tx_buf[VOFA_TX_BUF_SIZE];
    int len = snprintf(tx_buf, sizeof(tx_buf),
        "Gyro:%.2f,%.2f,%.2f\r\n",
        (double)yaw, (double)roll, (double)pitch);

    if (len > 0 && len < (int)sizeof(tx_buf)) {
        UartBase_SendStr(vofa_uart, tx_buf);
    }
}
