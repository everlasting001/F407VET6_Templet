## this markdown file is used to design the algorithm of the project
1.小车走直线，定速度，定位置
    1.1 双轮差速环PID
        通过计算和比较左轮和右轮的编码器distance，并结合PID算法（这里乘一个比例系数Alpha即可）
        参考如下：(这里的具体参数不要固定，根据实际情况调整)
'''c
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
'''
    1.2 位置控制
        通过编码器distance的平均值来判断是否到达目标位置，并结合PID算法进行速度调整
    1.3 速度控制
        采用PID控制算法并结合编码器数据，根据当前速度与目标速度的差值，计算出控制信号，控制电机转速。