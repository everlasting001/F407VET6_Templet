#ifndef __MY_I2C_H__
#define __MY_I2C_H__

#include "main.h"
#include "dwt_delay.h"

void MY_I2C_Init(void);
void MY_I2C_Start(void);
void MY_I2C_Stop(void);
void MY_I2C_SendByte(uint8_t byte);
uint8_t MY_I2C_ReceiveByte(void);
void MY_I2C_SendAck(uint8_t Ackbit);
uint8_t MY_I2C_ReceiveAck(void);

#endif 