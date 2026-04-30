#include "Encoder.h"
#include "main.h"
#include "tim.h"

void Encoder_Init(void){
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    
    encoder[LEFT_MOTOR_INDEX].motor_index = LEFT_MOTOR_INDEX;
    encoder[RIGHT_MOTOR_INDEX].motor_index = RIGHT_MOTOR_INDEX;
}

static void Encoder_GetPulse(encoder_t *encoder) {
    // 1. 读取计数器，并直接强制转换为 int16_t
    // 原理：丢弃高16位，只保留低16位，并将其解释为有符号数
    if(encoder->motor_index == LEFT_MOTOR_INDEX){
        encoder->current_cnt = (int16_t)__HAL_TIM_GetCounter(&htim3);
    } else {
        encoder->current_cnt = (int16_t)__HAL_TIM_GetCounter(&htim4);
    }

    // 2. 计算差值
    encoder->pulse_diff = encoder->current_cnt - encoder->last_cnt;
    
    // 3. 更新 last_cnt 供下次使用
    encoder->last_cnt = encoder->current_cnt;
    
    // 4. 累加脉冲
    encoder->pulse += encoder->pulse_diff;
}

static void Encoder_Update(encoder_t *encoder){
    Encoder_GetPulse(encoder);
    // 计算转速（单位：转/分钟）
    encoder->rpm = encoder->pulse_diff * 60.0f / 0.04 / PULSE_PER_ROUND;
    // 计算速度（单位：毫米/秒）
    encoder->mmps = encoder->rpm * WHEEL_CIRCUMFERENCE / 60.0f;
    // 计算距离（单位：毫米）
    encoder->distance += encoder->mmps*0.04;
} //定时周期：40ms

void Encoder_Read(void){
    Encoder_Update(&encoder[LEFT_MOTOR_INDEX]);
    Encoder_Update(&encoder[RIGHT_MOTOR_INDEX]);
}

void Encoder_Reset(void){
    __HAL_TIM_SetCounter(&htim3, 0);
    __HAL_TIM_SetCounter(&htim4, 0);

    encoder[LEFT_MOTOR_INDEX].pulse = 0;
    encoder[RIGHT_MOTOR_INDEX].pulse = 0;
    encoder[LEFT_MOTOR_INDEX].distance = 0;
    encoder[RIGHT_MOTOR_INDEX].distance = 0;
    encoder[LEFT_MOTOR_INDEX].last_cnt = 0;
    encoder[RIGHT_MOTOR_INDEX].last_cnt = 0;
    encoder[LEFT_MOTOR_INDEX].current_cnt = 0;
    encoder[RIGHT_MOTOR_INDEX].current_cnt = 0;
    encoder[LEFT_MOTOR_INDEX].pulse_diff = 0;
    encoder[RIGHT_MOTOR_INDEX].pulse_diff = 0;
    encoder[LEFT_MOTOR_INDEX].rpm = 0;
    encoder[RIGHT_MOTOR_INDEX].rpm = 0;
    encoder[LEFT_MOTOR_INDEX].mmps = 0;
    encoder[RIGHT_MOTOR_INDEX].mmps = 0;
}