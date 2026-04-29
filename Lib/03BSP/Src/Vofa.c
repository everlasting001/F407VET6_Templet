#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vofa.h"
#include "Variable.h"

float target;
/**
 * @brief 解析 Vofa 单行指令 (格式: Name:Value)
 * @param cmd_str 接收到的字符串, 例如 "Kp:2.5"
 */
void ParseVofaCommand(char* cmd_str) {
    // 1. 查找冒号 ':' 的位置
    char* separator = strchr(cmd_str, ':');
    if (separator == NULL) return; // 如果没有冒号, 格式错误

    // 2. 分割字符串: 将冒号改为字符串结束符 '\0'
    *separator = '\0';
    
    // 3. 获取键 (Key) 和值 (Value)
    char* key = cmd_str;          // 冒号前面的部分
    char* value_str = separator + 1; // 冒号后面的部分

    // 4. 去除 value_str 末尾可能存在的换行符 \r 或 \n
    char* pos;
    if ((pos = strchr(value_str, '\r')) != NULL) *pos = '\0';
    if ((pos = strchr(value_str, '\n')) != NULL) *pos = '\0';

    // 5. 匹配键并赋值
    // if (strcmp(key, "pos_Kp") == 0) {
    //     pid_position_controller.Kp = atof(value_str);
    // }
    // else if (strcmp(key, "pos_Ki") == 0) {
    //     pid_position_controller.Ki = atof(value_str);
    // }
    // else if (strcmp(key, "pos_Kd") == 0) {
    //     pid_position_controller.Kd = atof(value_str);
    // }
    // else if (strcmp(key, "vel_l_Kp") == 0) {
    //     pid_speed_controller[LEFT_MOTOR_INDEX].Kp = atof(value_str);
    // }
    // else if (strcmp(key, "vel_l_Ki") == 0) {
    //     pid_speed_controller[LEFT_MOTOR_INDEX].Ki = atof(value_str);
    // }
    // else if (strcmp(key, "vel_l_Kd") == 0) {
    //     pid_speed_controller[LEFT_MOTOR_INDEX].Kd = atof(value_str);
    // }
    // else if (strcmp(key, "vel_r_Kp") == 0) {
    //     pid_speed_controller[RIGHT_MOTOR_INDEX].Kp = atof(value_str);
    // }
    // else if (strcmp(key, "vel_r_Ki") == 0) {
    //     pid_speed_controller[RIGHT_MOTOR_INDEX].Ki = atof(value_str);
    // }
    // else if (strcmp(key, "vel_r_Kd") == 0) {
    //     pid_speed_controller[RIGHT_MOTOR_INDEX].Kd = atof(value_str);
    // }
    // else if (strcmp(key, "Target") == 0) {
    //     target = atof(value_str);
    // }
}