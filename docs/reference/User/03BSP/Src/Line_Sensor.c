#include "Line_Sensor.h"
#include "main.h"
#include "dwt_delay.h"

static void Select_Channel(uint8_t channel){
    HAL_GPIO_WritePin(AD0_GPIO_Port, AD0_Pin, (channel >> 0) & 0x01); // bit0 -> AD0
    HAL_GPIO_WritePin(AD1_GPIO_Port, AD1_Pin, (channel >> 1) & 0x01); // bit1 -> AD1
    HAL_GPIO_WritePin(AD2_GPIO_Port, AD2_Pin, (channel >> 2) & 0x01); // bit2 -> AD2
}
void Grayscale_Sensor_Read_All(void){
    uint8_t i;
    for (i = 0; i < GRAYSCALE_SENSOR_CHANNELS; i++){
        Select_Channel(i);
        Delay_us(50);
        sensor_values[i] = HAL_GPIO_ReadPin(OUT_GPIO_Port, OUT_Pin);
    }
}