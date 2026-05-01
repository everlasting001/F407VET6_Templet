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
