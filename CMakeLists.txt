cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME F407VET6_Templet)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Device layer
    Devices/Src/DeviceClass/Modules/ModuleBase.c # 模块基类
    Devices/Src/DeviceClass/Modules/LED.c # LED模块
    Devices/Src/DeviceClass/Modules/Buzzer.c # Buzzer模块
    Devices/Src/DeviceClass/Modules/Key.c # Key模块
    Devices/Src/DeviceClass/Motors/MotorBase.c # 电机基类
    Devices/Src/DeviceClass/Motors/StepperMotor.c # 步进电机
    Devices/Src/DeviceClass/Motors/DCMotor.c # DC有刷电机
    Devices/Src/DeviceClass/Sensors/SensorBase.c # 传感器基类
    Devices/Src/DeviceClass/Sensors/Encoder.c # 编码器传感器
    Devices/Src/DeviceClass/Sensors/LineSensor.c # 八路灰度循迹传感器
    Devices/Src/DeviceClass/Sensors/Gyro.c # MPU6050陀螺仪传感器
    Devices/Src/DeviceClass/Sensors/LengthMeasure.c # 长度测量模块
    Devices/Src/DebugPeripheral/dwt_delay.c # DWT微秒延时
    Devices/Src/DebugPeripheral/UartBase.c # UART基类
    Devices/Src/DebugPeripheral/DebugPrintf.c # 调试格式化输出
    Devices/Src/FilterAlgorithm/PID.c # PID控制器
    Devices/Src/FilterAlgorithm/Vofa.c # Vofa+ FireWater调参

    # Framework layer
    Framework/Src/MoveControl.c # 运动控制系统
    Framework/Src/Init.c # 全局初始化

    # Application layer
    Application/Src/Callback.c # 回调函数
    Application/Src/TaskSelector.c # 按键选择任务模块
    Application/Src/TaskProgram/Task1_LineTrack.c # Task1: 正方形边框逆时针循迹一圈
    Application/Src/TaskProgram/Task2_LineTrack.c # Task2: 正方形边框顺时针循迹一圈
    Application/Src/TaskProgram/Task3_LineTrack.c # Task3: 正方形边框循迹一圈
    Application/Src/TaskProgram/Task4_LineTrack.c # Task4: 180°掉头→巡线→第1边沿90°转→400pwm/500ms
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Device layer    
    Devices/Inc/DeviceClass/Modules
    Devices/Inc/DeviceClass/Motors
    Devices/Inc/DeviceClass/Sensors
    Devices/Inc/DebugPeripheral
    Devices/Inc/FilterAlgorithm

    # Framework layer
    Framework/Inc

    #application layer
    Application/Inc/TestProgram
    Application/Inc/TaskProgram
    Application/Inc/
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
