#include "MY_I2C.h"
#include <stdint.h>

//bit 1:释放SDA/SCL线 bit 0:拉低SDA/SCL线

// 写 SCL 引脚
static void MY_I2C_W_SCL(uint8_t bit){
    HAL_GPIO_WritePin(MCU6050_SCL_GPIO_Port, MCU6050_SCL_Pin, bit);
    Delay_us(5);
}

// 写 SDA 引脚
static void MY_I2C_W_SDA(uint8_t bit){
    HAL_GPIO_WritePin(MCU6050_SDA_GPIO_Port, MCU6050_SDA_Pin, bit);
    Delay_us(5);
}

// 读 SDA 引脚状态
static uint8_t MY_I2C_R_SDA(void){
    uint8_t bit;
    bit = HAL_GPIO_ReadPin(MCU6050_SDA_GPIO_Port, MCU6050_SDA_Pin);
    Delay_us(5);
    return bit;
}

void MY_I2C_Init(void)
{
    HAL_GPIO_WritePin(MCU6050_SCL_GPIO_Port, MCU6050_SCL_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MCU6050_SDA_GPIO_Port, MCU6050_SDA_Pin, GPIO_PIN_SET);
}

/*
SCL：迟到早退
SDA：早出晚归
*/

void MY_I2C_Start(void)
{
    MY_I2C_W_SDA(1);
    MY_I2C_W_SCL(1);
    MY_I2C_W_SDA(0);
    MY_I2C_W_SCL(0);
}

void MY_I2C_Stop(void)
{
    MY_I2C_W_SDA(0);
    MY_I2C_W_SCL(1);
    MY_I2C_W_SDA(1);
}

void MY_I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    for(i=0;i<8;i++)
    {
        MY_I2C_W_SDA(byte&(0x80>>i));
        MY_I2C_W_SCL(1);
        MY_I2C_W_SCL(0);
    }
}

uint8_t MY_I2C_ReceiveByte(void)
{
    uint8_t byte = 0x00;
    uint8_t i;
    for(i=0;i<8;i++){
        byte <<= 1; // 左移一位，为下一位腾出位置
        MY_I2C_W_SCL(1);
        byte |= MY_I2C_R_SDA(); // 读取SDA引脚状态，将其赋值给byte的最低位
        MY_I2C_W_SCL(0);
        /*
        receive byte:1001 1100
        i=0 byte:0000 0000 -> 0000 0001
        i=1 byte:0000 0001 -> 0000 0010
        i=2 byte:0000 0011 -> 0000 0100
        i=3 byte:0000 0111 -> 0000 1001
        i=4 byte:0000 1111 -> 0001 0011
        i=5 byte:0001 1111 -> 0010 0111
        i=6 byte:0011 1111 -> 0100 1110
        i=7 byte:0111 1111 -> 1001 1100
        */
    }
    return byte;
}

void MY_I2C_SendAck(uint8_t Ackbit)
{
    MY_I2C_W_SDA(Ackbit);
    MY_I2C_W_SCL(1);
    MY_I2C_W_SCL(0);
}

uint8_t MY_I2C_ReceiveAck(void)
{
    uint8_t Ackbit;
    MY_I2C_W_SDA(1);
    MY_I2C_W_SCL(1);
    Ackbit = MY_I2C_R_SDA();
    MY_I2C_W_SCL(0);
    return Ackbit;
}