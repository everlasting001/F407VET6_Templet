#include "Serial.h"
#include "Variable.h"
#include "usart.h"
#include "tim.h"

void SendUART1(void){
    if(Flag_SendUART1){
        /*双环位置式PID*/
        // sprintf((char*)SendData,"%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
        //     pid_pos.output,
        //     pid_pos.feedback,
        //     pid_pos.error,
        //     pid_pos.integral,

        //     pid_vel_l.output,
        //     pid_vel_l.feedback,
        //     pid_vel_l.error,
        //     pid_vel_l.integral,

        //     pid_vel_r.output,
        //     pid_vel_r.feedback,
        //     pid_vel_r.error,
        //     pid_vel_r.integral
        // );
        // HAL_UART_Transmit_IT(&huart1, SendData, strlen((char*)SendData));
        
        /*MPU6050偏航角*/
        // sprintf((char*)SendData,"%.2f,%.2f\r\n",
        //      pose.yaw,pose.gyro_z);
        // HAL_UART_Transmit_IT(&huart1, SendData, strlen((char*)SendData));
        
        /*Line Sensor灰度巡线*/
        // HAL_UART_Transmit_IT(&huart1,sensor_values, 8);
    }
}

void ReceiveUART1(void){
    if(Flag_ReceiveUART1){
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, receiveData, sizeof(receiveData));
        ParseVofaCommand((char*)receiveData);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}
