# F407VET6_Templet
这个项目是一个基于F407VET6的模板项目，用于快速搭建电赛F407VET6环境。

系统架构（System Architecture）：
    分为三层，分别是：
        设备层（Device Layer）
        框架层（Framework Layer）
        应用层（Application Layer）
该系统以面向对象的方式组织，每个设备都有一个对应的类，通过继承和组合的方式实现功能。

1.设备层（Device Layer）：
    分为三个部分；
        1.设备的类（Device Class）
            主干是三大基类：
                1.Modules
                    引申的子类有：
                        1.LED
                        2.KEY
                        3.OLED
                        4.BUZZER
                2.Sensors
                    引申的子类有：
                        1.Encoder(编码器)
                        2.Gyro(陀螺仪)-MPU6050
                        3.LineSensor(灰度循迹模块)
                        4.VisionSensor(视觉模块)-K230 这里使用Python实现，并使用API或者UART实现通信
                3.Motors
                    引申的子类有：
                        1.DCMotor(直流电机)
                        2.StepMotor(步进电机)
                        3.Servo
        2.调试外设（Debug Peripheral）
            分为四类：
                    1.UART调试
                    2.OLED打印
                    3.SPI调试
                    4.I2C调试
        3.滤波算法（Filter Algorithm）
                主要是PID算法和传感器滤波算法
2.框架层（Framework Layer）：
    分为多个系统：
        1.通信系统（Communication System）
            主要是UART通信系统，用于与PC端以及K230进行通信
        2.运动控制系统（Motion Control System）
            主要是PID控制器，用于控制电机的运动
        3.状态机系统（State Machine System）
            主要是状态机，用于控制系统的运行状态
3.应用层（Application Layer）：
    1.主程序（Main Program）
        主要是系统的主程序，用于初始化系统、运行状态机、与PC端进行通信等
    2.中断回调函数（Interrupt Callback Function）
        主要是处理中断的函数，用于响应外部事件，如按键按下、编码器旋转等
        主要是系统的核心，用于实现系统的功能
    3.测试程序（Test Program）
        主要是测试系统的功能，用于验证系统的功能是否正常
    4.任务集成实现（Task Integration Implementation）
        主要是将系统的不同功能集成起来，实现系统的完整功能
