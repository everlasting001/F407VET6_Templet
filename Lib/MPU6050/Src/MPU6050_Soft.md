#include "MPU6050.h"
#include "MY_I2C.h"
#include <stdint.h>
#include "OLED.h"
#include "Config.h"

#define MPU6050_ADDRESS (0x68 << 1)

void MPU6050_Init(pose_t *pose){
    // 初始化 I2C 接口
    MY_I2C_Init();
    MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);
    MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x09);
    MPU6050_WriteReg(MPU6050_CONFIG, 0x06); // 配置低通滤波器，带宽为 42Hz
    //在传感器数据采集（如加速度计、陀螺仪）中，42Hz 的低通滤波可以去除高频的随机噪声，保留信号的长期趋势，让数据曲线看起来更平滑。
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x00); // 陀螺仪满量程为 ±250 度/秒
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00); // 加速度计满量程为 ±2g

    pose->roll = 0.0f;
    pose->pitch = 0.0f;
    pose->yaw = 0.0f;
}

// 向 MPU6050 写入寄存器
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data){
    MY_I2C_Start();
    MY_I2C_SendByte(MPU6050_ADDRESS);
    MY_I2C_ReceiveAck();
    MY_I2C_SendByte(RegAddress);
    MY_I2C_ReceiveAck();
    MY_I2C_SendByte(Data);
    MY_I2C_ReceiveAck();
    MY_I2C_Stop();
}

// 从 MPU6050 读取寄存器
uint8_t MPU6050_ReadReg(uint8_t RegAddress){
    uint8_t Data;
    
    MY_I2C_Start();
    MY_I2C_SendByte(MPU6050_ADDRESS); // 发送写地址
    MY_I2C_ReceiveAck();
    MY_I2C_SendByte(RegAddress);
    MY_I2C_ReceiveAck();

    MY_I2C_Start();
    MY_I2C_SendByte(MPU6050_ADDRESS | 0x01); // 发送读地址，重置读写位
    MY_I2C_ReceiveAck();
    Data = MY_I2C_ReceiveByte();
    MY_I2C_SendAck(1);  // 发送 NACK 表示结束
    MY_I2C_Stop();

    return Data;
}

uint8_t MPU6050_GetID(void){
    return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

static void MPU6050_DAC_Data(pose_t *pose){
    pose->acc_x = (pose->acc_rx) / 32768.0f * 2.0f;
    pose->acc_y = (pose->acc_ry) / 32768.0f * 2.0f;
    pose->acc_z = (pose->acc_rz) / 32768.0f * 2.0f;

    pose->gyro_x = (pose->gyro_rx) / 32768.0f * 250.0f;
    pose->gyro_y = (pose->gyro_ry) / 32768.0f * 250.0f;
    pose->gyro_z = (pose->gyro_rz) / 32768.0f * 250.0f;
}

void MPU6050_GetData(pose_t *pose){
    uint8_t Data_H, Data_L;
    // 读取加速度计数据
    Data_H = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L);
    pose->acc_rx = (Data_H << 8) | Data_L;
    Data_H = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L);
    pose->acc_ry = (Data_H << 8) | Data_L;
    Data_H = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L);
    pose->acc_rz = (Data_H << 8) | Data_L;
    // 读取陀螺仪数据
    Data_H = MPU6050_ReadReg(MPU6050_GYRO_XOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_GYRO_XOUT_L);
    pose->gyro_rx = (Data_H << 8) | Data_L;
    Data_H = MPU6050_ReadReg(MPU6050_GYRO_YOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_GYRO_YOUT_L);
    pose->gyro_ry = (Data_H << 8) | Data_L;
    Data_H = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
    pose->gyro_rz = (Data_H << 8) | Data_L;

    MPU6050_DAC_Data(pose);
}
