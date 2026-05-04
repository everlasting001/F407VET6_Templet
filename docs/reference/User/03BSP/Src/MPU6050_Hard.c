#include "MPU6050_Hard.h"
#include "i2c.h"
#include "main.h"
#include <stdint.h>
#include <math.h>

#define MPU6050_ADDRESS (0x68 << 1)

// 向 MPU6050 写入一个字节
static HAL_StatusTypeDef MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data) {
    return HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, &Data, 1, 100);
}

// 从 MPU6050 读取一个字节
static uint8_t MPU6050_ReadReg(uint8_t RegAddress) {
    uint8_t Data = 0;
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDRESS, RegAddress, I2C_MEMADD_SIZE_8BIT, &Data, 1, 100);
    return Data;
}

void MPU6050_Init(pose_t *pose){
    MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);
    MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x09);
    /*
    ±250 °/s	0x00	131.0	精度最高，适合测量缓慢的动作（如人体姿态）
    ±500 °/s	0x08	65.5	平衡点，兼顾精度与量程（常用）
    ±1000 °/s	0x10	32.8	量程较大，适合测量较快的动作
    ±2000 °/s	0x18	16.4	精度最低，但量程最大，适合测量剧烈动作
    */
    MPU6050_WriteReg(MPU6050_CONFIG, 0x06); // 配置低通滤波器，带宽为 42Hz
    //在传感器数据采集（如加速度计、陀螺仪）中，42Hz 的低通滤波可以去除高频的随机噪声，保留信号的长期趋势，让数据曲线看起来更平滑。
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x08); // 陀螺仪满量程为 ±500 度/秒
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00); // 加速度计满量程为 ±2g

    pose->roll = 0.0f;
    pose->pitch = 0.0f;
    pose->yaw = 0.0f;
}

uint8_t MPU6050_GetID(void){
    return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

static void MPU6050_DAC_Data(pose_t *pose){
    // pose->acc_x = (pose->acc_rx) / 32768.0f * 2.0f;
    // pose->acc_y = (pose->acc_ry) / 32768.0f * 2.0f;
    // pose->acc_z = (pose->acc_rz) / 32768.0f * 2.0f;

    // pose->gyro_x = (pose->gyro_rx) / 32768.0f * 250.0f + 2.0f;
    // pose->gyro_y = (pose->gyro_ry) / 32768.0f * 250.0f + 1.5f;
    pose->gyro_z = (pose->gyro_rz) / 32768.0f * 500.0f + 0.4f;
}

static void MPU6050_GetData(pose_t *pose){
    uint8_t Data_H, Data_L;
    // // 读取加速度计数据
    // Data_H = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H);
    // Data_L = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L);
    // pose->acc_rx = (Data_H << 8) | Data_L;
    // Data_H = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H);
    // Data_L = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L);
    // pose->acc_ry = (Data_H << 8) | Data_L;
    // Data_H = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H);
    // Data_L = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L);
    // pose->acc_rz = (Data_H << 8) | Data_L;
    // // 读取陀螺仪数据
    // Data_H = MPU6050_ReadReg(MPU6050_GYRO_XOUT_H);
    // Data_L = MPU6050_ReadReg(MPU6050_GYRO_XOUT_L);
    // pose->gyro_rx = (Data_H << 8) | Data_L;
    // Data_H = MPU6050_ReadReg(MPU6050_GYRO_YOUT_H);
    // Data_L = MPU6050_ReadReg(MPU6050_GYRO_YOUT_L);
    // pose->gyro_ry = (Data_H << 8) | Data_L;
    Data_H = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
    Data_L = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
    pose->gyro_rz = (Data_H << 8) | Data_L;

    MPU6050_DAC_Data(pose);
}

float NormalizeAngle(float angle) {
    // 先映射到 [0, 360)
    angle = fmod(angle + 180.0f, 360.0f);
    
    // 如果是负数，fmod 可能处理得不一样，确保它是正的
    if (angle < 0) {
        angle += 360.0f;
    }
    
    // 再移回 [-180, 180)
    if (angle >= 180.0f) {
        angle -= 360.0f;
    }
    
    return angle;
}

void MPU6050_PoseUpdate(pose_t *pose, float dt){
    // if(pose->gyro_x < 0.15f && pose->gyro_x > -0.15f){
    //     pose->pitch += 0;    
    // }else{
    //     pose->pitch += (pose->gyro_x) * dt;
    // }
    // if(pose->gyro_y < 0.15f && pose->gyro_y > -0.15f){
    //     pose->roll += 0;    
    // }else{
    //     pose->roll += (pose->gyro_y) * dt;
    // }
    MPU6050_GetData(pose);
    pose->Flag_Gyro_Start = 1;
    if(pose->Flag_Gyro_Start){
        if(pose->gyro_z < 0.40f && pose->gyro_z > -0.40f){
            pose->yaw += 0;    
        }else{
            pose->yaw += (pose->gyro_z) * dt;
        }
    }
    // pose->yaw = NormalizeAngle(pose->yaw);
}

void MPU6050_Reset(void){
    pose.roll = 0.0f;
    pose.pitch = 0.0f;
    pose.yaw = 0.0f;

    pose.gyro_x = 0.0f;
    pose.gyro_y = 0.0f;
    pose.gyro_z = 0.0f;
    
    pose.gyro_rx = 0.0f;
    pose.gyro_ry = 0.0f;
    pose.gyro_rz = 0.0f;

    pose.Flag_Gyro_Start = 0;
}