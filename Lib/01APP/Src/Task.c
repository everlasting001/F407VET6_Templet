#include "Task.h"
#include "CallBack.h"
#include "Variable.h"
#include "stm32f1xx_hal_uart.h"
#include "usart.h"
#include "tim.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

void Task_Init(void){
    HAL_TIM_Base_Start_IT(&htim1);
    DWT_Delay_Init();
    Encoder_Init();
    OLED_Init();
    Motor_Init();
    MPU6050_Init(&pose);
    Task1_Init(&task1);
    Task2_Init(&task2);
    Task3_Init(&task3);
    Task4_Init(&task4);
}

void Task_Test(void){
    // Task1_StateMachine(&task1);
    // Task2_StateMachine(&task2);
    Task3_StateMachine(&task3);
    // Task4_StateMachine(&task4);
}

void Task_Function(void);
void Task_Main(void);
void Task_Callback(void);
void Task_SendUart1(void){
    char uart1_buf[128];
    sprintf(uart1_buf, "yaw: %f\r\n", pose.yaw);
    HAL_UART_Transmit_IT(&huart1, (uint8_t*)uart1_buf, strlen(uart1_buf));
}