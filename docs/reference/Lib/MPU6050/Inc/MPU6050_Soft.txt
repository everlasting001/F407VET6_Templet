#ifndef __MPU6050_H__
#define __MPU6050_H__

// 采样率分频寄存器
#include "Config.h"
#define MPU6050_SMPLRT_DIV 0x19
// 配置寄存器
#define MPU6050_CONFIG 0x1A
// 陀螺仪配置寄存器
#define MPU6050_GYRO_CONFIG 0x1B
// 加速度传感器配置寄存器
#define MPU6050_ACCEL_CONFIG 0x1C
// 加速度传感器寄存器
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_ACCEL_XOUT_L 0x3C
#define MPU6050_ACCEL_YOUT_H 0x3D
#define MPU6050_ACCEL_YOUT_L 0x3E
#define MPU6050_ACCEL_ZOUT_H 0x3F
#define MPU6050_ACCEL_ZOUT_L 0x40
// 温度传感器寄存器
#define MPU6050_TEMP_OUT_H 0x41
#define MPU6050_TEMP_OUT_L 0x42
// 陀螺仪传感器寄存器
#define MPU6050_GYRO_XOUT_H 0x43
#define MPU6050_GYRO_XOUT_L 0x44
#define MPU6050_GYRO_YOUT_H 0x45
#define MPU6050_GYRO_YOUT_L 0x46
#define MPU6050_GYRO_ZOUT_H 0x47
#define MPU6050_GYRO_ZOUT_L 0x48
// 电源管理寄存器
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_PWR_MGMT_2 0x6C
// 设备ID寄存器
#define MPU6050_WHO_AM_I 0x75

#include "main.h"

void MPU6050_Init(pose_t *pose);
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data);
uint8_t MPU6050_ReadReg(uint8_t RegAddress);
void MPU6050_GetData(pose_t *pose);
uint8_t MPU6050_GetID(void);

#endif