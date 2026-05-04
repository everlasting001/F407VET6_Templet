#include "Move_Control.h"
#include "Config.h"
#include "Motor.h"
#include "Variable.h"
#include "pid.h" 
#include <math.h>
#include <stdint.h>

static float BALANCE_Kp = 0.5f;
static float BALANCE_Kd = 0.2f;
static float last_distance_diff = 0;
// static float SPIN_Kp = 0.5f;
// static float SPIN_Kd = 0.0f;

void Car_Reset(uint8_t flag){
    if(flag == 0){
        return;
    }else{
        Encoder_Reset();
        MPU6050_Reset();
        Motor_Stop(LEFT_MOTOR_INDEX);
        Motor_Stop(RIGHT_MOTOR_INDEX);
    }
}

uint8_t BangBang_Straight_mm(float distance_mm){
    // --- 1. 参数定义 ---
    int16_t max_pwm = 48;        // 最大 PWM
    int16_t min_run_pwm = 15;    // 维持转动的最小 PWM (防止堵转)
    float stop_threshold = 5.0f; // 停止的距离阈值 (mm)
    float slow_down_start;
    if(distance_mm < 750.0f || distance_mm > -750.0f){
        slow_down_start = distance_mm * 0.35f; // 开始减速的距离 (mm)，根据惯性调整    
    }else if(((distance_mm >= 750.0f) && ((distance_mm <= 1500.0f))) || ((distance_mm <= -750.0f) && (distance_mm >= -1500.0f))){
        slow_down_start = distance_mm * 0.25f;
    }else{
        slow_down_start = distance_mm * 0.15f;
    }
    
    // --- 2. 反馈获取 ---
    float dis_l = encoder[LEFT_MOTOR_INDEX].distance;
    float dis_r = encoder[RIGHT_MOTOR_INDEX].distance;
    float actual_mm = (dis_l + dis_r) * 0.5f;
    
    // 计算距离目标的剩余误差
    float position_error = distance_mm - actual_mm;
    
    // --- 3. 速度规划 (关键修改) ---
    int16_t basic_pwm;
    
    if(fabs(position_error) < stop_threshold){
        // 【阶段1】进入停止阈值：直接刹车
        Motor_Stop(LEFT_MOTOR_INDEX);
        Motor_Stop(RIGHT_MOTOR_INDEX);
        last_distance_diff = 0; // 清零微分项历史值，防止积分饱和/微分爆炸
        return 1; // 直接退出函数
    }
    else if(fabs(position_error) < slow_down_start){
        // 【阶段2】进入减速区：线性减速
        // 这样可以避免冲过头，并且在低速时有足够时间修正方向
        float ratio = fabs(position_error) / slow_down_start; // 0 ~ 1
        basic_pwm = min_run_pwm + (max_pwm - min_run_pwm) * ratio;

        if(position_error < 0){
            basic_pwm = -basic_pwm;
        }
    }
    else{
        // 【阶段3】正常行驶：全速前进/后退
        basic_pwm = (position_error > 0) ? max_pwm : -max_pwm;
    }

    // --- 4. 差速转向修正 (PD控制保持不变) ---
    float dist_diff = encoder[RIGHT_MOTOR_INDEX].distance - encoder[LEFT_MOTOR_INDEX].distance;
    float derivative = dist_diff - last_distance_diff;
    last_distance_diff = dist_diff;
    
    float turn_correction = dist_diff * BALANCE_Kp + derivative * BALANCE_Kd;

    // --- 5. 输出限制 (防止 PWM 超出驱动范围) ---
    // 假设驱动器限制为 [-50, 50]
    float pwm_limit = 50.0f;
    float pwm_left;
    float pwm_right;
    
    pwm_left = basic_pwm + turn_correction;
    pwm_right = basic_pwm - turn_correction;  
    
    // 简单限幅
    if(pwm_left > pwm_limit) pwm_left = pwm_limit;
    if(pwm_left < -pwm_limit) pwm_left = -pwm_limit;
    if(pwm_right > pwm_limit) pwm_right = pwm_limit;
    if(pwm_right < -pwm_limit) pwm_right = -pwm_limit;

    // --- 6. 执行 ---
    Motor_SetPWM((int16_t)pwm_left, LEFT_MOTOR_INDEX);
    Motor_SetPWM((int16_t)pwm_right, RIGHT_MOTOR_INDEX);

    return 0;
}

uint8_t BangBang_Spin_angle(uint8_t mode,float target_angle){
    // --- 1. 参数定义 ---
    int16_t max_pwm = (mode == Spin_Mode_Center) ? 25 : 50;        // 最大 PWM
    int16_t min_run_pwm = 15;    // 维持转动的最小 PWM
    float stop_threshold = 1.0f; // 停止的角度阈值 (degree)
    float slow_down_start = fabs(target_angle); // 开始减速的角度 (degree)，根据惯性调整

    // --- 2. 反馈获取 ---
    float yaw = pose.yaw;
     // 计算角度误差
    float angle_error = target_angle - yaw;
    // 处理跨越 ±180° 的情况（非常重要！）
    if (angle_error > 180.0f) angle_error -= 360.0f;
    if (angle_error < -180.0f) angle_error += 360.0f;

    // --- 3. 速度规划 ---
    int16_t pwm;
    if(fabs(angle_error) < stop_threshold){
        // 【阶段1】进入停止阈值：直接刹车
        Motor_Stop(LEFT_MOTOR_INDEX);
        Motor_Stop(RIGHT_MOTOR_INDEX);
        return 1;
    }else if(fabs(angle_error) < slow_down_start){
        // 【阶段2】进入减速区：线性减速
        float ratio = fabs(angle_error) / slow_down_start; // 0 ~ 1
        if(target_angle > 0){
            pwm = (angle_error > 0) ? min_run_pwm + (max_pwm - min_run_pwm) * ratio : -min_run_pwm - (max_pwm - min_run_pwm) * ratio;    
        }else{
            pwm = (angle_error > 0) ? -min_run_pwm - (max_pwm - min_run_pwm) * ratio : min_run_pwm + (max_pwm - min_run_pwm) * ratio;    
        }
    }else{
        // 【阶段2】正常旋转：全速旋转
        if(target_angle > 0){
            pwm = (angle_error > 0) ? max_pwm : -max_pwm;
        }else{
            pwm = (angle_error > 0) ? -max_pwm : max_pwm;
        }
    }
    switch(mode){
        case Spin_Mode_Edge:
            if(target_angle > 0){
                Spin_Left(pwm);
                Motor_Stop(LEFT_MOTOR_INDEX);
            }
            else{
                Spin_Right(pwm);
                Motor_Stop(RIGHT_MOTOR_INDEX);
            }
            break;
        case Spin_Mode_Center:
            if(target_angle > 0){
                Spin_Left(pwm);
                Spin_Right(-pwm);
            }
            else{
                Spin_Right(pwm);
                Spin_Left(-pwm);
            }
            break;
    }
    return 0;
}

//转弯半径R = (vR + vL) * L/(vR - vL) * 2

uint8_t BangBang_Revolve_angle(float target_angle){
    // --- 1. 参数定义 ---
    int16_t pwm_fast_max = 50;
    int16_t pwm_slow_max = 20;
    int16_t pwm_fast_min = 11;
    int16_t pwm_slow_min = 11;
    float pwm_fast;
    float pwm_slow;

    float stop_threshold = 1.0f;
    float slow_down_start = 30.0f;

    // --- 2. 反馈获取 ---
    float yaw = pose.yaw;
     // 计算角度误差
    float angle_error = target_angle - yaw;
    // --- 3. 速度规划 ---
    if(fabs(angle_error) < stop_threshold){
        // 【阶段1】进入停止阈值：直接刹车
        Motor_Stop(LEFT_MOTOR_INDEX);
        Motor_Stop(RIGHT_MOTOR_INDEX);
        return 1;
    }else if(fabs(angle_error) < slow_down_start){
        // 【阶段2】进入减速区：线性减速
        float ratio = fabs(angle_error) / slow_down_start; // 0 ~ 1
        if(target_angle > 0){
            pwm_fast = (angle_error > 0) ? pwm_fast_min + (pwm_fast_max - pwm_fast_min) * ratio : -pwm_fast_min - (pwm_fast_max - pwm_fast_min) * ratio;
            pwm_slow = (angle_error > 0) ? pwm_slow_min + (pwm_slow_max - pwm_slow_min) * ratio : -pwm_slow_min - (pwm_slow_max - pwm_slow_min) * ratio;
        }else{
            pwm_fast = (angle_error > 0) ? -pwm_fast_min - (pwm_fast_max - pwm_fast_min) * ratio : pwm_fast_min + (pwm_fast_max - pwm_fast_min) * ratio;
            pwm_slow = (angle_error > 0) ? -pwm_slow_min - (pwm_slow_max - pwm_slow_min) * ratio : pwm_slow_min + (pwm_slow_max - pwm_slow_min) * ratio;
        }
    }else{
        // 【阶段3】正常旋转：全速旋转
        if(target_angle > 0){
            if(angle_error > 0){
                pwm_fast = pwm_fast_max;
                pwm_slow = pwm_slow_max;
            }else{
                pwm_fast = -pwm_fast_max;
                pwm_slow = -pwm_slow_max;
            }
        }else{
            if(angle_error > 0){
                pwm_fast = -pwm_fast_max;
                pwm_slow = -pwm_slow_max;
            }else{
                pwm_fast = pwm_fast_max;
                pwm_slow = pwm_slow_max;
            }
        }
    }           
    // --- 4. 执行 ---
    if(target_angle > 0){
        Motor_SetPWM(pwm_fast, RIGHT_MOTOR_INDEX);
        Motor_SetPWM(pwm_slow, LEFT_MOTOR_INDEX);
    }else{
        Motor_SetPWM(pwm_fast, LEFT_MOTOR_INDEX);
        Motor_SetPWM(pwm_slow, RIGHT_MOTOR_INDEX);
    }
    return 0;
}