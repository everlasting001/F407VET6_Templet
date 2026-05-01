## This text is designed to put some hardware configuration information.
1. TIM Configuration
    - TIM1&&TIM8: Encoder Mode(TIM1 -> Left_DCMotor, TIM8 -> Right_DCMotor)(Encoder Mode TI1 AND TI2),四倍频
    - TIM2: Global Timer(全局定时中断)
    - TIM3: PWM generation(CH1->Left_DCMotor,CH2->Right_DCMotor)
2. Motor Configuration
    There are two sort of motors in this project: DC Motor and Stepper Motor.
    1. DC Motor
        型号： JGA25-310
        传感器类型： 霍尔编码器
        电机类型： 有刷直流电机
        磁环线数： 13
        减速比： 1:20
        额定转速： 440-460rpm
        额定电压： 7.4V
        使用电机驱动板：TB6612FNG(见电机驱动板配置)
        引脚配置：
            1.Left_DCMotor: 
                1.用于控制电机正反转的引脚：
                    AIN1->PD0
                    AIN2->PD1
                2.PWM输出引脚：
                    PWMA->PA6->TIM3_CH1
                3.编码器引脚：
                    E1A->PE9->TIM1_CH1(编码器A相)
                    E1B->PE11->TIM1_CH2(编码器B相)
            2.Right_DCMotor:
                1.用于控制电机正反转的引脚：
                    AIN1->PD2
                    AIN2->PD3
                2.PWM输出引脚：
                    PWMA->PA7->TIM3_CH2
                3.编码器引脚：
                    E1A->PC6->TIM8_CH1(编码器A相)
                    E1B->PC7->TIM8_CH2(编码器B相)
    2. Stepper Motor
        型号：28BYJ-48(四相八拍步进电机)
        驱动板：ULN2003(见驱动板配置)
        - Horizontal_Stepper_Motor:
            引脚配置：
                四个相引脚：
                    H_IN1->PD4
                    H_IN2->PD5
                    H_IN3->PD6
                    H_IN4->PD7
        - Vertical_Stepper_Motor:
            引脚配置：
                四个相引脚：
                    V_IN1->PB3
                    V_IN2->PB4
                    V_IN3->PB5
                    V_IN4->PB8
3. Car Configuration
    ENCODER_LINE        13        // 编码器线数
    GEAR_RATIO          20.409f       // 减速比
    MOTOR_COUNT        2        // 电机数量(左轮前进：逆时针转；右轮前进顺时针转)
    PULSE_PER_ROUND    (ENCODER_LINE * 4 * GEAR_RATIO)  // 四倍频后单圈脉冲数（实际意义上的一圈）
    WHEEL_DIAMETER           65.0f    // 车轮直径(mm)
    WHEEL_CIRCUMFERENCE      (WHEEL_DIAMETER * 3.1415926f) // 车轮周长(mm)
    WHEEL_BASE_DISTANCE      125.0f   // 轮基距离(mm)
4. LineSensor Configuration
    这是一个八路灰度循迹传感器，传感器的8个探测通道间距为11.5mm
    传感器通过片选引脚的方式给出各个通道的检测结果，且使用二进制比特位的方式表示片选
    引脚配置：
        AD0->PD12
        AD1->PD13
        AD2->PD14
        OUT->PD11
    gpio配置上AD0-AD2为推挽输出，对应通过更改传感器上对应的引脚来选择不同的通道
    通道片选表：
        从左到右8个通道：
              AD2   AD1   AD0
        CH1    0     0     0
        CH2    0     0     1
        CH3    0     1     0
        CH4    0     1     1
        CH5    1     0     0
        CH6    1     0     1
        CH7    1     1     0
        CH8    1     1     1
    gpio配置上OUT为输入，对应传感器的输出引脚，用于接收传感器的检测结果
    使用方式是写一个for循环，每次循环选择一个通道，读取OUT引脚的电平，根据电平判断该通道是否被检测到
5.