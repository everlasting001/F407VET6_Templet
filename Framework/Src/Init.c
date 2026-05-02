#include "Init.h"
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "SensorBase.h"
#include "MotorBase.h"
#include "Vofa.h"

/* ==================== 电机引脚配置 ==================== */

static const DCMotorPinConfig left_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {AIN1_GPIO_Port, AIN1_Pin},   /* AIN1 → PD0 */
    [DCMOTOR_PIN_AIN2] = {AIN2_GPIO_Port, AIN2_Pin},   /* AIN2 → PD1 */
};

static const DCMotorPinConfig right_pins[DCMOTOR_PIN_COUNT] = {
    [DCMOTOR_PIN_AIN1] = {BIN1_GPIO_Port, BIN1_Pin},   /* BIN1 → PD2 */
    [DCMOTOR_PIN_AIN2] = {BIN2_GPIO_Port, BIN2_Pin},   /* BIN2 → PD3 */
};

/* ==================== 调试串口 DMA 接收缓冲 ==================== */

static uint8_t dbg_rx_buffer[512];

/* ==================== 全局实例定义 ==================== */

DebugPrintf_t  dbg_printf;

Encoder_t      left_encoder;
Encoder_t      right_encoder;
DCMotor_t      left_motor;
DCMotor_t      right_motor;
MoveControl_t  move_ctrl;
LineSensor_t  line_sensor;
Gyro_t        gyro;

/* ==================== Framework_Init — 一站式初始化 ==================== */

/**
  * @brief  Framework 层一站式初始化
  *
  * 按顺序完成:
  *   1. DebugPrintf  — 构造调试串口, 启动 DMA+IDLE 接收, 输出启动信息
  *   2. Encoder      — 构造左右编码器 (TIM1/TIM8 编码器模式), 启动计数
  *   3. DCMotor      — 构造左右电机 (TIM3 CH1/CH2 PWM), 启动 PWM
  *   4. MoveControl  — 绑定编码器+电机, 设置 PID 参数, 清零编码器
  *
  * @note  后续新增模块 (MPU6050, 灰度传感器, 步进电机 等) 在此按顺序添加
  */
void Framework_Init(void)
{
    /* ==================== 1. DebugPrintf 调试串口 ==================== */

    DebugPrintf_Constructor(&dbg_printf, &huart1,
                            dbg_rx_buffer, sizeof(dbg_rx_buffer));
    if (DebugPrintf_Init(&dbg_printf) != 0) {
        Error_Handler();
    }

    DebugPrintf_Print(&dbg_printf,
        "=== Framework Init Start ===\r\n");

    /* ==================== 2. 编码器 ==================== */

    /* 左编码器: TIM1, 索引 0, 极性反转 (A/B 相接反) */
    Encoder_Constructor(&left_encoder, &htim1, 0, -1);
    SensorBase_Init((SensorBase_t *)&left_encoder);

    /* 右编码器: TIM8, 索引 1, 极性正常 */
    Encoder_Constructor(&right_encoder, &htim8, 1, +1);
    SensorBase_Init((SensorBase_t *)&right_encoder);

    DebugPrintf_Print(&dbg_printf,
        "  Encoder: L(TIM1) R(TIM8) OK\r\n");

    /* ==================== 3. 电机 ==================== */

    /* 左电机: TIM3_CH1 (PA6) */
    DCMotor_Constructor(&left_motor, "Left_DCMotor", left_pins,
                        &htim3, TIM_CHANNEL_1);
    Motor_Init(&left_motor.base);

    /* 右电机: TIM3_CH2 (PA7) */
    DCMotor_Constructor(&right_motor, "Right_DCMotor", right_pins,
                        &htim3, TIM_CHANNEL_2);
    Motor_Init(&right_motor.base);

    DebugPrintf_Print(&dbg_printf,
        "  DCMotor: L(TIM3_CH1) R(TIM3_CH2) OK\r\n");

    /* ==================== 4. 运动控制 ==================== */

    MoveControl_Init(&move_ctrl,
                     &left_motor, &right_motor,
                     &left_encoder, &right_encoder);

    Encoder_HardReset(&left_encoder);
    Encoder_HardReset(&right_encoder);

    DebugPrintf_Print(&dbg_printf,
        "  MoveControl: Cascade PID OK\r\n");

    /* ==================== 5. 灰度循迹传感器 ==================== */

    LineSensor_Constructor(&line_sensor, GPIOD,
                           GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14,
                           GPIOD, GPIO_PIN_11);
    SensorBase_Init((SensorBase_t *)&line_sensor);

    DebugPrintf_Print(&dbg_printf,
        "  LineSensor: 8ch OK\r\n");

    /* ==================== 6. MPU6050 陀螺仪 ==================== */

    Gyro_Constructor(&gyro, &hi2c1);
    if (SensorBase_Init((SensorBase_t *)&gyro) == 0) {
        uint8_t id = Gyro_GetDeviceID(&gyro);
        DebugPrintf_Print(&dbg_printf,
            "  Gyro: MPU6050 ID=0x%02X OK\r\n", id);
    } else {
        DebugPrintf_Print(&dbg_printf,
            "  Gyro: MPU6050 Init FAILED\r\n");
    }

    Vofa_SetGyro(&gyro);

    /* ==================== 7. 后续模块初始化预留 ==================== */
    /*
     * TODO: 按需添加以下模块初始化:
     *
     *   StepperMotor_Init(&stepper);
     *   Servo_Init(&servo);
     *   ...
     */

    DebugPrintf_Print(&dbg_printf,
        "=== Framework Init Done ===\r\n");
}

/* ==================== Framework_IRQHandler — 中断调度 ==================== */

/**
  * @brief  Framework 层中断调度 (25Hz / 40ms)
  *
  * 在 Callback.c 的 HAL_TIM_PeriodElapsedCallback 中由软件分频调用。
  * 执行顺序:
  *   1. SensorBase_Run: 更新左右编码器数据 (脉冲 → RPM → mmps → distance_mm)
  *   2. MoveControl_RunPositionMode: 位置控制 (BangBang + PD 差速修正)
  *
  * @note  保持快速返回 (< 100us), 不在 ISR 中阻塞或打印
  */
void Framework_IRQHandler(void)
{
    /* 运行级联 PID 运动控制 (位置环 → 差速修正 → 速度环 → PWM) */
    MoveControl_Update(&move_ctrl);
}
