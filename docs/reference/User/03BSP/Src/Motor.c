#include "Motor.h"
#include "Config.h"
#include "main.h"
#include "tim.h"

void Motor_Init(void){
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
}

void Motor_SetPWM(int16_t PWM,MotorIndex_e motor){
    switch(motor){
        case LEFT_MOTOR_INDEX:
            if (PWM >= 0) {
                // 正转
                HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
                __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_1, (int8_t)PWM);
                break;
            } else {
                // 反转 (注意：GPIO 变化，但 PWM 值始终为正)
                HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
                __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_1, (int8_t)(-PWM));
                break;
            }
        case RIGHT_MOTOR_INDEX:
            if (PWM >= 0) {
                // 正转
                HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_SET);
                __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_2, (int8_t)PWM);
                break;
            } else {
                // 反转 (注意：GPIO 变化，但 PWM 值始终为正)
                HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
                __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_2, (int8_t)(-PWM));
                break;
            }
    }
}

void Motor_Stop(MotorIndex_e motor){
    switch(motor){
        case LEFT_MOTOR_INDEX:
            __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_1, 0);
            break;
        case RIGHT_MOTOR_INDEX:
            __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_2, 0);
            break;
    }
}

void Spin_Left(uint16_t PWM){
    Motor_SetPWM(PWM, RIGHT_MOTOR_INDEX);
}

void Spin_Right(uint16_t PWM){
    Motor_SetPWM(PWM, LEFT_MOTOR_INDEX);
}

// void Motor_SetPWM_L(int16_t PWM) {
//     if (PWM >= 0) {
//             // 正转
//             HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_RESET);
//             HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_SET);
//             __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_1, (int8_t)PWM);
//     } else {
//         // 反转 (注意：GPIO 变化，但 PWM 值始终为正)
//         HAL_GPIO_WritePin(AIN1_GPIO_Port, AIN1_Pin, GPIO_PIN_SET);
//         HAL_GPIO_WritePin(AIN2_GPIO_Port, AIN2_Pin, GPIO_PIN_RESET);
//         __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_1, (int8_t)(-PWM));
//     }
// }

// void Motor_SetPWM_R(int16_t PWM) {
//     if (PWM >= 0) {
//             // 正转
//             HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_SET);
//             HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_RESET);
//             __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_2, (int8_t)PWM);
//     } else {
//         // 反转 (注意：GPIO 变化，但 PWM 值始终为正)
//         HAL_GPIO_WritePin(BIN1_GPIO_Port, BIN1_Pin, GPIO_PIN_RESET);
//         HAL_GPIO_WritePin(BIN2_GPIO_Port, BIN2_Pin, GPIO_PIN_SET);
//         __HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_2, (int8_t)(-PWM));
//     }
// }

// void Motor_Stop(PID_Controller_t *pid){
//     pid->output = 0;
// }