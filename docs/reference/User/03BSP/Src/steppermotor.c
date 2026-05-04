#include "28BYJ48.h"
#include "dwt_delay.h"
#include "main.h"
/*
整步驱动（四拍）：每步 11.25° → 一圈 = 360 ÷ 11.25 = 32 步
半步驱动（八拍）：每步 5.625° → 一圈 = 360 ÷ 5.625 = 64 步

实际减速比 ≈ 1/63.68395（通常近似为 1/64）。
作用：降低转速、增加扭矩，使电机能驱动小型机械负载。
加上减速齿轮后，输出轴一圈所需的步数大幅增加：

整步驱动：
32×64≈2048 步/圈
半步驱动：
64×64≈4096 步/圈

控制 28BYJ-28 就是控制它的 线圈通电顺序。常见的驱动方式有两种：
单相激励：一次只给一个线圈通电，省电但力矩小。
双相激励：同时给两个线圈通电，力矩大，转动更平稳。
半步模式：单相和双相交替，既保证力矩又能提高精度。
以半步模式为例，通电顺序如下（对应 IN1-IN4）：
1 0 0 0
1 1 0 0
0 1 0 0
0 1 1 0
0 0 1 0
0 0 1 1
0 0 0 1
1 0 0 1
这 8 个状态循环一次，就是电机转动 8 步。
*/
static uint8_t StepSequence[8][4] ={
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

// 私有辅助函数：设置电机线圈电平
static void SetMotorPins(StepMotor_t* motor,uint8_t in1,uint8_t in2,uint8_t in3,uint8_t in4){
    HAL_GPIO_WritePin(motor->pins.IN1_Port, motor->pins.IN1_Pin, in1);
    HAL_GPIO_WritePin(motor->pins.IN2_Port, motor->pins.IN2_Pin, in2);
    HAL_GPIO_WritePin(motor->pins.IN3_Port, motor->pins.IN3_Pin, in3);
    HAL_GPIO_WritePin(motor->pins.IN4_Port, motor->pins.IN4_Pin, in4);
}

// 私有辅助函数：根据索引设置步进
static void SetStep(StepMotor_t* motor, uint8_t step){
    const uint8_t* pattern = StepSequence[step];
    SetMotorPins(motor, pattern[0], pattern[1], pattern[2], pattern[3]);
}

// 方法实现：初始化
void StepMotor_Init(StepMotor_t* motor, MotorPins_t* pinConfig) {
    // 复制引脚配置
    motor->pins = *pinConfig;
    motor->stepindex = HORIZEN;
    motor->speed = MEDIUM; // 默认速度
    motor->dir = FW; // 默认正转
    // 初始化引脚为低电平
    SetMotorPins(motor, 0, 0, 0, 0);
}

// 方法实现：运行
void StepMotor_Run(StepMotor_t* motor, uint8_t dir, uint16_t angle) {
    // 计算总步数 (4096步/360度)
    uint16_t totalSteps = (uint16_t)(angle * 4096.0f / 360.0f);
    
    for (uint16_t i = 0; i < totalSteps; i++) {
        // 执行当前步
        SetStep(motor, motor->stepindex);
        
        // 更新步进索引（方向控制）
        if (dir > 0) {
            motor->stepindex = (motor->stepindex + 1) % 8; // 正转
        } else {
            motor->stepindex = (motor->stepindex + 7) % 8; // 反转 (避免负数)
        }
        // 控制转速，调整延时以适应不同负载和要求
        if(motor->speed == SLOW){
            Delay_us(1500);
        }else if(motor->speed == MEDIUM){
            Delay_us(800);
        }else if(motor->speed == FAST){
            Delay_us(500);
        }
        // 可选：在步与步之间短暂断电或保持（根据驱动器特性调整）
        SetMotorPins(motor, 0, 0, 0, 0); 
        // 如果需要更平滑，可以取消上面这行注释实现“空步”，或者保持通电以维持扭矩
    }
    
    // 完成后保持最后状态或断电
    SetMotorPins(motor, 0, 0, 0, 0); 
}

// 将对象和引脚配置移到这里，并标记为 static，仅本文件可见
static StepMotor_t MotorH;
static StepMotor_t MotorV;

static const MotorPins_t pinConfog_H = {
    .IN1_Port = IN1_Horizen_GPIO_Port,
    .IN1_Pin = IN1_Horizen_Pin,
    .IN2_Port = IN2_Horizen_GPIO_Port,
    .IN2_Pin = IN2_Horizen_Pin,
    .IN3_Port = IN3_Horizen_GPIO_Port,
    .IN3_Pin = IN3_Horizen_Pin,
    .IN4_Port = IN4_Horizen_GPIO_Port,
    .IN4_Pin = IN4_Horizen_Pin,
};
static const MotorPins_t pinConfig_V = {
    .IN1_Port = IN1_Vertical_GPIO_Port,
    .IN1_Pin = IN1_Vertical_Pin,
    .IN2_Port = IN2_Vertical_GPIO_Port,
    .IN2_Pin = IN2_Vertical_Pin,
    .IN3_Port = IN3_Vertical_GPIO_Port,
    .IN3_Pin = IN3_Vertical_Pin,
    .IN4_Port = IN4_Vertical_GPIO_Port,
    .IN4_Pin = IN4_Vertical_Pin,
};
// // 步进电机运行函数
// // dir: 1=正转, 0=反转; angle: 转动角度
// void STEPMOTOR_Run(uint8_t dir, uint16_t angle, uint8_t motor_index){
//     // 28BYJ-48实际每圈需要4096步（360/5.625=64 *64）
//     // 90度 = 4096 / 4 = 1024步
//     uint16_t totalSteps = (uint16_t)(angle * 4096 / 360);
//     uint8_t stepIndex = 0;
//     switch(motor_index){
//         case MOTOR_HORIZONTAL:
//             for (uint16_t i = 0; i < totalSteps; i++){
//                 // 清除当前输出
//                 HAL_GPIO_WritePin(IN1_Horizen_GPIO_Port, IN1_Horizen_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN2_Horizen_GPIO_Port, IN2_Horizen_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN3_Horizen_GPIO_Port, IN3_Horizen_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN4_Horizen_GPIO_Port, IN4_Horizen_Pin, GPIO_PIN_RESET);
//                 // 设置当前步的电平
//                 if (StepSequence[stepIndex][0])
//                     HAL_GPIO_WritePin(IN1_Horizen_GPIO_Port, IN1_Horizen_Pin, GPIO_PIN_SET);
//                 if (StepSequence[stepIndex][1])
//                     HAL_GPIO_WritePin(IN2_Horizen_GPIO_Port, IN2_Horizen_Pin, GPIO_PIN_SET);
//                 if (StepSequence[stepIndex][2])
//                     HAL_GPIO_WritePin(IN3_Horizen_GPIO_Port, IN3_Horizen_Pin, GPIO_PIN_SET);
//                 if (StepSequence[stepIndex][3])
//                     HAL_GPIO_WritePin(IN4_Horizen_GPIO_Port, IN4_Horizen_Pin, GPIO_PIN_SET);
//                 // 控制方向
//                 if (dir > 0){
//                     stepIndex = (stepIndex + 1) % 8;
//                 // 正转
//                 }else{
//                     stepIndex = (stepIndex + 7) % 8;
//                 // 反转（避免负数）
//                 }   
//                 Delay_us(800);
//                 // 控制转速，可根据需要调整
//                 HAL_GPIO_WritePin(IN1_Horizen_GPIO_Port, IN1_Horizen_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN2_Horizen_GPIO_Port, IN2_Horizen_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN3_Horizen_GPIO_Port, IN3_Horizen_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN4_Horizen_GPIO_Port, IN4_Horizen_Pin, GPIO_PIN_RESET);
//             }
//                 break;
//         case MOTOR_VERTICAL:
//             for (uint16_t i = 0; i < totalSteps; i++){
//                 HAL_GPIO_WritePin(IN1_Vertical_GPIO_Port, IN1_Vertical_Pin, StepSequence[stepIndex][0]);
//                 HAL_GPIO_WritePin(IN2_Vertical_GPIO_Port, IN2_Vertical_Pin, StepSequence[stepIndex][1]);
//                 HAL_GPIO_WritePin(IN3_Vertical_GPIO_Port, IN3_Vertical_Pin, StepSequence[stepIndex][2]);
//                 HAL_GPIO_WritePin(IN4_Vertical_GPIO_Port, IN4_Vertical_Pin, GPIO_PIN_RESET);
//                 // 设置当前步的电平
//                 if (StepSequence[stepIndex][0])
//                     HAL_GPIO_WritePin(IN1_Vertical_GPIO_Port, IN1_Vertical_Pin, GPIO_PIN_SET);
//                 if (StepSequence[stepIndex][1])                    
//                     HAL_GPIO_WritePin(IN2_Vertical_GPIO_Port, IN2_Vertical_Pin, GPIO_PIN_SET);
//                 if (StepSequence[stepIndex][2])
//                     HAL_GPIO_WritePin(IN3_Vertical_GPIO_Port, IN3_Vertical_Pin, GPIO_PIN_SET);
//                 if (StepSequence[stepIndex][3])
//                     HAL_GPIO_WritePin(IN4_Vertical_GPIO_Port, IN4_Vertical_Pin, GPIO_PIN_SET);
//                 // 控制方向
//                 if (dir > 0){
//                     stepIndex = (stepIndex + 1) % 8;
//                 // 正转
//                 }else{
//                     stepIndex = (stepIndex + 7) % 8;
//                 // 反转（避免负数）
//                 }   
//                 Delay_us(800);
//                 // 控制转速，可根据需要调整
//                 HAL_GPIO_WritePin(IN1_Vertical_GPIO_Port, IN1_Vertical_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN2_Vertical_GPIO_Port, IN2_Vertical_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN3_Vertical_GPIO_Port, IN3_Vertical_Pin, GPIO_PIN_RESET);
//                 HAL_GPIO_WritePin(IN4_Vertical_GPIO_Port, IN4_Vertical_Pin, GPIO_PIN_RESET);
//             }
//                 break;
//     }
// }