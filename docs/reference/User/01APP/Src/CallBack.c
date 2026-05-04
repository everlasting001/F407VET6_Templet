#include "CallBack.h"
#include "Task.h" 
#include "Move_Control.h"
#include "Variable.h"
#include "usart.h"
#include "tim.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* HAL库回调函数重定义 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
    if(htim == &htim1){
        Key_Tick();
        static uint16_t Cnt1 = 0;
        static uint16_t Cnt2 = 0;
        static uint16_t Cnt3 = 0;
        Cnt1++;
        Cnt2++;
        Cnt3++;
        if(Cnt1 >= 40){
            Encoder_Read();
            Cnt1 = 0;
        }//40ms
        if(Cnt2 >= 40){
            Flag_SendUART1 = 1;
            Flag_ReceiveUART1 = 1;
            Cnt2 = 0;
        }//200ms
        if(Cnt3 >= 2){
            MPU6050_PoseUpdate(&pose, 0.002f);
            Cnt3 = 0;
        }//2ms
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
    if(huart == &huart1){
        Flag_SendUART1 = 0;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
    if(huart == &huart1){
        Flag_ReceiveUART1 = 0;
    }
}