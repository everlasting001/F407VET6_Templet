/*位置式实现思路*/

公式：
out = Kp*error(t) + Ki*∑error(k) + Kd*(error(t)-error(t-1));

变量：
float Target,Out,Actual;
float Kp,Ki,Kd;
float Error,PreError,Integral;

主函数：
int main(){
    HAL_TIM_Base_Start_IT();
    while(1){
        Target = 用户设定;
    }
}

中断回调函数：
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
    if(TIM_HandleTypeDef == &htimx){
        Actual = 传感器(这里单独写一个函数);
        
        PreError = Error;
        Error = Target - Actual;
        
        Integral += Error;
        
        Out = Kp*Error + Ki*Integral + Kd*(Error-PreError);
        
        if(Out>Max) Out = Max;
        if(Out<-Max) Out = -Max;

        Out输出到被控对象（作为一个函数参量）；
    }
}
/*增量式实现思路*/
公式：
out = Kp*(error(t)-error(t-1)) + Ki*error(t) + Kd*(error(t)-2*error(t-1)+error(t-2));

变量：
float Target,Out,Actual,DeltaOut;
float Kp,Ki,Kd;
float Error[3];（这一次，上一次，上上次）

主函数：
int main(){
    HAL_TIM_Base_Start_IT();
    while(1){
        Target = 用户设定;
    }
}

中断回调函数：
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
    if(TIM_HandleTypeDef == &htimx){
        Actual = 传感器(这里单独写一个函数);
        
        Error[2] = Error[1];
        Error[1] = Error[0];
        Error[0] = Target - Actual;
        
        Out += Kp*(Error[0] - Error[1]) + Ki*Error[0] + Kd*(Error[0]-2*Error[1]+Error[2]);
        DeltaOut = Kp*(Error[0] - Error[1]) + Ki*Error[0] + Kd*(Error[0]-2*Error[1]+Error[2]);

        if(Out>Max) Out = Max;
        if(Out<-Max) Out = -Max;

        Out输出到被控对象（作为一个函数参量）；
    }
}

1.位置式定位置控制一般不用i项；只用p项就能很好解决，但是p项过大时会导致响应过慢，p项过小时会导致震荡；引入d项的目的是消除过冲
2.增量式定位置控制一定要有i项，否则会出现很大的偏移，同时增量式的p项也与历史的out有关

PID算法改进：
    积分限幅：限制积分幅度，防止积分深度饱和
    积分分离：误差小于一个限度才开始积分，反之积分项为0
    变速积分：根据误差大小调整积分的速度
    不完全微分（微分滤波）：给微分项加一个一阶惯性单元（低通滤波器）
    微分先行：将对误差的微分替换为对实际值的微分
    输出偏移：在非零输出时，给输出项一个固定偏移
    输入死区：误差小于一个限度时不进行调控

奇怪的机制：
左轮pid和右轮pid必须分开计算，否则会出现数据覆盖等奇怪问题？

//以下是速度pid版本

#if 0
static void Init_MoveControl(void){
    pid_controller[LEFT_MOTOR_INDEX].type = PID_TYPE_POSITION;
    pid_controller[RIGHT_MOTOR_INDEX].type = PID_TYPE_POSITION;
    // 初始化PID控制器
    PID_Init(&pid_controller[LEFT_MOTOR_INDEX],pid_controller[LEFT_MOTOR_INDEX].type);
    PID_Init(&pid_controller[RIGHT_MOTOR_INDEX], pid_controller[RIGHT_MOTOR_INDEX].type);
    // 初始化编码器
    encoder[LEFT_MOTOR_INDEX].motor_index = LEFT_MOTOR_INDEX;
    encoder[LEFT_MOTOR_INDEX].pulse = 0;
    encoder[LEFT_MOTOR_INDEX].distance = 0;
        
    encoder[RIGHT_MOTOR_INDEX].motor_index = RIGHT_MOTOR_INDEX;
    encoder[RIGHT_MOTOR_INDEX].pulse = 0;
    encoder[RIGHT_MOTOR_INDEX].distance = 0;

    // 初始化PID控制器参数 - 改进参数
    pid_controller[LEFT_MOTOR_INDEX].Kp = 0.15f;  // 提高比例增益
    pid_controller[LEFT_MOTOR_INDEX].Ki = 0.10f; // 降低积分增益
    pid_controller[LEFT_MOTOR_INDEX].Kd = 0.02f;  // 提高微分增益
    pid_controller[LEFT_MOTOR_INDEX].output_max = 100.0f; // 提高输出上限
    pid_controller[LEFT_MOTOR_INDEX].output_min = -100.0f; // 提高输出下限
    pid_controller[LEFT_MOTOR_INDEX].enable_output_limit = 1;
    pid_controller[LEFT_MOTOR_INDEX].enable_integral_limit = 0;
    pid_controller[LEFT_MOTOR_INDEX].integral_max = 200.0f; // 调整积分限幅
    pid_controller[LEFT_MOTOR_INDEX].integral_min = -200.0f;
    pid_controller[LEFT_MOTOR_INDEX].enable_derivative_filter = 1;
    pid_controller[LEFT_MOTOR_INDEX].derivative_filter_alpha = 0.7f; // 调整滤波系数
    pid_controller[LEFT_MOTOR_INDEX].enable_integral_separation = 1;
    pid_controller[LEFT_MOTOR_INDEX].integral_separation_threshold = 200.0f;
    
    pid_controller[RIGHT_MOTOR_INDEX].Kp = 0.15f;  // 提高比例增益
    pid_controller[RIGHT_MOTOR_INDEX].Ki = 0.10f; // 降低积分增益
    pid_controller[RIGHT_MOTOR_INDEX].Kd = 0.02f;  // 提高微分增益
    pid_controller[RIGHT_MOTOR_INDEX].output_max = 100.0f; // 提高输出上限
    pid_controller[RIGHT_MOTOR_INDEX].output_min = -100.0f; // 提高输出下限
    pid_controller[RIGHT_MOTOR_INDEX].enable_output_limit = 1;
    pid_controller[RIGHT_MOTOR_INDEX].enable_integral_limit = 0;
    pid_controller[RIGHT_MOTOR_INDEX].integral_max = 200.0f; // 调整积分限幅
    pid_controller[RIGHT_MOTOR_INDEX].integral_min = -200.0f;
    pid_controller[RIGHT_MOTOR_INDEX].enable_derivative_filter = 1;
    pid_controller[RIGHT_MOTOR_INDEX].derivative_filter_alpha = 0.7f; // 调整滤波系数
    pid_controller[RIGHT_MOTOR_INDEX].enable_integral_separation = 1;
    pid_controller[RIGHT_MOTOR_INDEX].integral_separation_threshold = 200.0f;
}

void Task_Test_Encoder(void){
    Encoder_Update(&encoder[LEFT_MOTOR_INDEX]);
    Encoder_Update(&encoder[RIGHT_MOTOR_INDEX]);
}

void Task_Test_PID(void){
    pid_controller[LEFT_MOTOR_INDEX].output = PID_Calculate(100,encoder[LEFT_MOTOR_INDEX].rpm,&pid_controller[LEFT_MOTOR_INDEX]);
    pid_controller[RIGHT_MOTOR_INDEX].output = PID_Calculate(100,encoder[RIGHT_MOTOR_INDEX].rpm,&pid_controller[RIGHT_MOTOR_INDEX]);

    PWM_L = (int16_t)pid_controller[LEFT_MOTOR_INDEX].output;
    Motor_SetPWM_L(PWM_L);
    PWM_R = (int16_t)pid_controller[RIGHT_MOTOR_INDEX].output;
    Motor_SetPWM_R(PWM_R);
}
 
#endif
//以下是里程pid版本

#if 1
static void Init_MoveControl(void){
    // 初始化PID控制器
    PID_Init(&pid_controller[LEFT_MOTOR_INDEX], PID_TYPE_POSITION);
    PID_Init(&pid_controller[RIGHT_MOTOR_INDEX], PID_TYPE_POSITION);

    // 初始化编码器
    encoder[LEFT_MOTOR_INDEX].motor_index = LEFT_MOTOR_INDEX;
    encoder[LEFT_MOTOR_INDEX].pulse = 0;
    encoder[LEFT_MOTOR_INDEX].distance = 0;
        
    encoder[RIGHT_MOTOR_INDEX].motor_index = RIGHT_MOTOR_INDEX;
    encoder[RIGHT_MOTOR_INDEX].pulse = 0;
    encoder[RIGHT_MOTOR_INDEX].distance = 0;

    // 初始化PID控制器参数 - 改进参数
    pid_controller[LEFT_MOTOR_INDEX].type = PID_TYPE_POSITION;
    pid_controller[LEFT_MOTOR_INDEX].Kp = 0.17f;  // 提高比例增益
    pid_controller[LEFT_MOTOR_INDEX].Ki = 0.1f; // 降低积分增益
    pid_controller[LEFT_MOTOR_INDEX].Kd = 0.1f;  // 提高微分增益
    pid_controller[LEFT_MOTOR_INDEX].output_max = 40.0f; // 提高输出上限
    pid_controller[LEFT_MOTOR_INDEX].output_min = -40.0f; // 提高输出下限
    pid_controller[LEFT_MOTOR_INDEX].enable_output_limit = 1;
    pid_controller[LEFT_MOTOR_INDEX].enable_integral_limit = 1;
    pid_controller[LEFT_MOTOR_INDEX].integral_max = 200.0f; // 调整积分限幅
    pid_controller[LEFT_MOTOR_INDEX].integral_min = -200.0f;
    pid_controller[LEFT_MOTOR_INDEX].enable_derivative_filter = 1;
    pid_controller[LEFT_MOTOR_INDEX].derivative_filter_alpha = 0.5f; // 调整滤波系数
    pid_controller[LEFT_MOTOR_INDEX].enable_integral_separation = 1;
    pid_controller[LEFT_MOTOR_INDEX].integral_separation_threshold = 50.0f;
    pid_controller[LEFT_MOTOR_INDEX].enable_deadzone = 1;
    pid_controller[LEFT_MOTOR_INDEX].deadzone = 5.0f;

    pid_controller[RIGHT_MOTOR_INDEX].type = PID_TYPE_POSITION;
    pid_controller[RIGHT_MOTOR_INDEX].Kp = 0.17f;  // 提高比例增益
    pid_controller[RIGHT_MOTOR_INDEX].Ki = 0.1f; // 降低积分增益
    pid_controller[RIGHT_MOTOR_INDEX].Kd = 0.1f;  // 提高微分增益
    pid_controller[RIGHT_MOTOR_INDEX].output_max = 40.0f; // 提高输出上限
    pid_controller[RIGHT_MOTOR_INDEX].output_min = -40.0f; // 提高输出下限
    pid_controller[RIGHT_MOTOR_INDEX].enable_output_limit = 1;
    pid_controller[RIGHT_MOTOR_INDEX].enable_integral_limit = 1;
    pid_controller[RIGHT_MOTOR_INDEX].integral_max = 200.0f; // 调整积分限幅
    pid_controller[RIGHT_MOTOR_INDEX].integral_min = -200.0f;
    pid_controller[RIGHT_MOTOR_INDEX].enable_derivative_filter = 1;
    pid_controller[RIGHT_MOTOR_INDEX].derivative_filter_alpha = 0.5f; // 调整滤波系数
    pid_controller[RIGHT_MOTOR_INDEX].enable_integral_separation = 1;
    pid_controller[RIGHT_MOTOR_INDEX].integral_separation_threshold = 50.0f;
    pid_controller[RIGHT_MOTOR_INDEX].enable_deadzone = 1;
    pid_controller[RIGHT_MOTOR_INDEX].deadzone = 5.0f;
}

void Task_Test_PID(void){
    pid_controller[LEFT_MOTOR_INDEX].output = PID_Calculate(2000,encoder[LEFT_MOTOR_INDEX].distance,&pid_controller[LEFT_MOTOR_INDEX]);
    pid_controller[RIGHT_MOTOR_INDEX].output = PID_Calculate(2000,encoder[RIGHT_MOTOR_INDEX].distance,&pid_controller[RIGHT_MOTOR_INDEX]);

    PWM_L = (int16_t)pid_controller[LEFT_MOTOR_INDEX].output;
    Motor_SetPWM_L(PWM_L);
    PWM_R = (int16_t)pid_controller[RIGHT_MOTOR_INDEX].output;
    Motor_SetPWM_R(PWM_R);
}

#endif

void GoStraight_Control(void) {
     static float target_distance; // 目标距离

     if(Key_Check(Key1, KEY_SINGLE)){
         target_distance = 600.0f;
     }
     // 注意：Encoder_Update 应该在定时器中断里运行（如40ms）
     // 这里直接读取更新后的数据
     float dis_l = encoder[LEFT_MOTOR_INDEX].distance;  // 左轮距离
     float dis_r = encoder[RIGHT_MOTOR_INDEX].distance; // 右轮距离
     float vel_l = encoder[LEFT_MOTOR_INDEX].rpm;      // 左轮速度
     float vel_r = encoder[RIGHT_MOTOR_INDEX].rpm;      // 右轮速度

     // 外环计算 (位置环)
     // 目标：计算出期望的基础速度 (base_velocity)
     // 原理：基于左右轮平均位置的误差
     float current_avg = (dis_l + dis_r) * 0.5f; // 当前平均位置
     pid_pos.target = target_distance;
     pid_pos.feedback = current_avg;
    
     float base_velocity;
     base_velocity = PID_Calculate(pid_pos.target, pid_pos.feedback, &pid_pos);
    
     // 转向修正计算 (差速环)
     // 目标：计算出左右轮的速度差修正量，防止跑偏
     // 原理：如果右轮比左轮走的远(正差)，说明小车右偏，需要让左轮快一点，右轮慢一点
     float dist_diff = dis_r - dis_l; // 正数表示右轮多，负数表示左轮多
     float turn_correction = dist_diff * BALANCE_Kp; // P控制
        
     // 内环计算 (速度环 x 2)
     // 目标：将目标速度转换为 PWM
     // 左轮目标 = 基础速度 + 修正量 (如果右偏，修正量为正，左轮需加速)
     // 右轮目标 = 基础速度 - 修正量 (如果右偏，修正量为正，右轮需减速)
        
     // --- 左轮控制 ---
     pid_vel_l.target = base_velocity + turn_correction;
     pid_vel_l.feedback = vel_l;
     PID_Calculate(pid_vel_l.target, pid_vel_l.feedback, &pid_vel_l);
     pwm_l = (int16_t)pid_vel_l.output;
     Motor_SetPWM_L(pwm_l);
     // --- 右轮控制 ---
     pid_vel_r.target = base_velocity - turn_correction;
     pid_vel_r.feedback = vel_r;
     PID_Calculate(pid_vel_r.target, pid_vel_r.feedback, &pid_vel_r);
     pwm_r = (int16_t)pid_vel_r.output;
     Motor_SetPWM_R(pwm_r);
    
     // E. 结束判断 (到达目标)     // 如果误差很小且速度很低，则停止
     if(fabs(pid_pos.error) < 1.0f && fabs(base_velocity) < 3.0f) {
         Motor_Stop(&pid_vel_l);
         Motor_Stop(&pid_vel_r);
     }
}

// // 全局中间变量，用于连接三个环
// static float g_base_velocity = 0.0f;        // 位置环输出的基础速度
// static float g_turn_correction = 0.0f;      // 平衡环输出的修正量

// // 最终输出的目标速度（供速度环使用）
// static float target_left_vel = 0.0f;
// static float target_right_vel = 0.0f;

// /**
//  * @brief 位置环控制 (外环)
//  * @note  决定小车整体应该跑多快
//  */
// static void PositionLoop_Update(void) {
//     // 1. 读取数据
//     float dis_l = encoder[LEFT_MOTOR_INDEX].distance;
//     float dis_r = encoder[RIGHT_MOTOR_INDEX].distance;
    
//     // 2. 计算平均位置
//     float current_avg = (dis_l + dis_r) * 0.5f;
    
//     // 3. 更新PID参数并计算
//     pid_pos.target = target_distance; // 假设 target_distance 是全局目标
//     pid_pos.feedback = current_avg;
    
//     // 4. 计算并保存基础速度
//     g_base_velocity = PID_Calculate(pid_pos.target,pid_pos.feedback,&pid_pos); // 直接输出基础速度
// }

// /**
//  * @brief 平衡环/转向修正 (差速控制)
//  * @note  决定左右轮速应该相差多少来保持直线
//  */
// static void BalanceLoop_Update(void) {
//     // 1. 读取数据 (只关心距离差)
//     float dis_l = encoder[LEFT_MOTOR_INDEX].distance;
//     float dis_r = encoder[RIGHT_MOTOR_INDEX].distance;
    
//     // 2. 计算距离差 (右减左)
//     float dist_diff = dis_r - dis_l;
    
//     // 3. P控制计算修正量
//     // 如果右轮走多了(正数)，说明车向右偏，需要左轮加速/右轮减速
//     g_turn_correction = dist_diff * BALANCE_Kp; 
// }

// /**
//  * @brief 速度环控制 (内环)
//  * @note  分别计算左右轮的PWM
//  */ 
// static void VelocityLoop_Update(void) {
//     // --- 左轮独立计算 ---
//     {
//         // 1. 组合目标速度：基础速度 + 修正量 (修正右偏则左轮加速)
//         target_left_vel = g_base_velocity + g_turn_correction;
        
//         // 2. 读取左轮反馈
//         float vel_l = encoder[LEFT_MOTOR_INDEX].rpm;
        
//         // 3. 更新左轮PID并计算PWM
//         pid_vel_l.target = target_left_vel;
//         pid_vel_l.feedback = vel_l;
//         PID_Calculate(target_left_vel,vel_l,&pid_vel_l); // 计算左轮输出
        
//         // 4. 输出左轮PWM
//         pwm_l = (int16_t)pid_vel_l.output;
//         Motor_SetPWM_L(pwm_l);
//     } // 左轮作用域结束

//     // --- 右轮独立计算 ---
//     {
//         // 1. 组合目标速度：基础速度 - 修正量 (修正右偏则右轮减速)
//         target_right_vel = g_base_velocity - g_turn_correction;
        
//         // 2. 读取右轮反馈
//         float vel_r = encoder[RIGHT_MOTOR_INDEX].rpm;
        
//         // 3. 更新右轮PID并计算PWM
//         pid_vel_r.target = target_right_vel;
//         pid_vel_r.feedback = vel_r;
//         PID_Calculate(target_right_vel,vel_r,&pid_vel_r); // 计算右轮输出
        
//         // 4. 输出右轮PWM
//         pwm_r = (int16_t)pid_vel_r.output;
//         Motor_SetPWM_R(pwm_r);
//     } // 右轮作用域结束
// }

// void GoStraight_Control(void) {
    
//     target_distance = target;
//     // 三环严格顺序执行
//     PositionLoop_Update();  // 第一步：算总速
//     BalanceLoop_Update();   // 第二步：算修正
//     VelocityLoop_Update();  // 第三步：算PWM (左右彻底隔离)

//     //结束判断 
//     if(abs(pid_pos.error) < 10.0f && ((abs(pwm_r)) < 4.0f || (abs(pwm_l)) < 4.0f)) {
//         pwm_r = 0;
//         pwm_l = 0;
//         // 可选：复位PID积分项
//     }
// }
