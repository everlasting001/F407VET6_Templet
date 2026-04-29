#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

typedef enum {
    // 电机索引
    LEFT_MOTOR_INDEX = 0,
    RIGHT_MOTOR_INDEX = 1,
} MotorIndex_e;

typedef enum {
    // PID控制器类型
    PID_TYPE_POSITION = 0,  // 位置式PID
    PID_TYPE_INCREMENT      // 增量式PID
} PID_Type_e;

typedef enum{
    Normal = 0,
    Change = 1,
} Line_e;

typedef enum{
    Task_In = 0,
    Task_Out = 1,
    Task_Straight = 2,
    Task_Spin_Left = 3,
    Task_Spin_Right = 4,
    Task_Turn_Left = 5,
    Task_Turn_Right = 6,
    Task_Over = 7,
}Task_e;

typedef struct{
    Task_e task;
}Task_t;

typedef struct{
    // PID参数
    float Kp;               // 比例系数
    float Ki;               // 积分系数  
    float Kd;               // 微分系数
    
    // 控制目标和反馈
    float target;           // 目标值
    float feedback;         // 反馈值
    float last_feedback;    // 上次反馈值 (自动更新)
    float error;            // 当前误差 (自动计算)
    float last_error;       // 上次误差 (自动更新)
    float last_last_error;  // 上上次误差 (自动更新)
    
    // 积分相关
    float integral;         // 积分累积值 (自动累积)
    float integral_max;     // 积分限幅最大值
    float integral_min;     // 积分限幅最小值
    float integral_separation_threshold; // 积分分离阈值
    
    // 输出相关
    float output;           // PID输出 (自动计算)
    float output_offset;    // 输出偏移量
    float output_max;       // 输出限幅最大值
    float output_min;       // 输出限幅最小值
    float last_output;      // 上次输出 (自动更新)
    
    // 死区处理
    float deadzone;         // 输入死区大小
    
    // 控制标志
    PID_Type_e type;        // PID类型
    uint8_t enable_integral_separation; // 积分分离使能 (1=开启, 0=关闭)
    uint8_t enable_integral_limit;      // 积分限幅使能 (1=开启, 0=关闭)
    uint8_t enable_output_limit;        // 输出限幅使能 (1=开启, 0=关闭)
    uint8_t enable_deadzone;            // 死区使能 (1=开启, 0=关闭)
	uint8_t enable_derivative_primer; // 微分先行使能 (1=开启, 0=关闭)
    uint8_t enable_anti_windup;      // 抗积分饱和使能
    uint8_t enable_derivative_filter; // 微分滤波使能

    float derivative_filter_alpha;    // 微分滤波系数 (0-1)
    float last_derivative;           // 上次微分项值
    float filtered_derivative;       // 滤波后的微分项

} PID_Controller_t; // PID控制器结构体

typedef struct{
    MotorIndex_e motor_index; // 电机索引
    int16_t current_cnt; 
    int16_t last_cnt;
    int32_t pulse;
    int32_t pulse_diff;
    float rpm;
    float mmps; //单位：毫米/秒
    float distance; //单位：毫米    
} encoder_t; // 编码器结构体

typedef struct{
    int16_t acc_rx; // 加速度计x轴寄存器值
    int16_t acc_ry; // 加速度计y轴寄存器值
    int16_t acc_rz; // 加速度计z轴寄存器值

    int16_t gyro_rx; // 陀螺仪x轴寄存器值
    int16_t gyro_ry; // 陀螺仪y轴寄存器值
    int16_t gyro_rz; // 陀螺仪z轴寄存器值

    float acc_x; // 加速度计x轴 单位：g
    float acc_y; // 加速度计y轴 单位：g
    float acc_z; // 加速度计z轴 单位：g

    float gyro_x; // 陀螺仪x轴 单位：°/s
    float gyro_y; // 陀螺仪y轴 单位：°/s
    float gyro_z; // 陀螺仪z轴 单位：°/s

    float roll; // 滚转角(°)
    float pitch; // 俯仰角(°)
    float yaw; // 偏航角(°)

    uint8_t Flag_Gyro_Start; // 陀螺仪积分标志
} pose_t; // 物体姿态结构体

//硬件配置

// ============ 请在这里切换电机型号 ============
//#define MOTOR_TYPE_JGA370 // 选择JGA-370
#define MOTOR_TYPE_MG310 // 选择MG-310
// ==========================================

#ifdef MOTOR_TYPE_JGA370
    #define ENCODER_LINE 11
    #define GEAR_RATIO   9.6f
#elif defined(MOTOR_TYPE_MG310)
    #define ENCODER_LINE 13
    #define GEAR_RATIO   20.409f
#else
    #error "未定义电机型号！请在上方选择电机类型。"
#endif

#define MOTOR_COUNT        2        // 电机数量(左轮前进：逆时针转；右轮前进顺时针转)
#define PULSE_PER_ROUND    (ENCODER_LINE * 4 * GEAR_RATIO)  // 四倍频后单圈脉冲数（实际意义上的一圈）
#define WHEEL_DIAMETER           65.0f    // 车轮直径(mm)
#define WHEEL_CIRCUMFERENCE      (WHEEL_DIAMETER * 3.1415926f) // 车轮周长(mm)
#define WHEEL_BASE_DISTANCE      125.0f   // 轮基距离(mm)

#endif