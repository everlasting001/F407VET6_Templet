import sys

path = r'c:\Users\yinyi\Desktop\STM32_HAL_PROJECT\F407VET6\F407VET6_Templet\Framework\Src\MoveControl.c'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

changes = 0

# === 1. Add turn PID defines after DEFAULT_TURN_KP ===
old1 = '#define DEFAULT_TURN_KP                 20.0f   /**< 转弯 P 增益 (PWM/°) */\n#define DEFAULT_ADJUST_DISTANCE_MM      80.0f   /**< 微调前进距离 (传感器到轮轴) */'
new1 = '#define DEFAULT_TURN_KP                 20.0f   /**< 转弯 P 增益 (PWM/°) */\n#define DEFAULT_TURN_KI                 2.00f   /**< 转弯 I 增益 */\n#define DEFAULT_TURN_KD                 5.00f   /**< 转弯 D 增益 */\n#define DEFAULT_TURN_INTEGRAL_LIM       200.0f  /**< 转弯积分限幅 */\n#define DEFAULT_ADJUST_DISTANCE_MM      80.0f   /**< 微调前进距离 (传感器到轮轴) */'

if old1 in content:
    content = content.replace(old1, new1)
    changes += 1
    print('OK: added turn PID defines')
else:
    print('MISS 1: turn PID defines')
    # debug: show what's there
    idx = content.find('#define DEFAULT_TURN_KP')
    if idx >= 0:
        print(repr(content[idx:idx+200]))

# === 2. Add PID_Init for turn_pid after turn_kp init ===
old2 = '    ctrl->turn_kp                  = DEFAULT_TURN_KP;\n    ctrl->adjust_distance_mm       = DEFAULT_ADJUST_DISTANCE_MM;'
new2 = '    ctrl->turn_kp                  = DEFAULT_TURN_KP;\n    PID_Init(&ctrl->turn_pid, DEFAULT_TURN_KP, DEFAULT_TURN_KI, DEFAULT_TURN_KD,\n             DEFAULT_TURN_INTEGRAL_LIM, DEFAULT_TURN_PWM);\n    ctrl->adjust_distance_mm       = DEFAULT_ADJUST_DISTANCE_MM;'

if old2 in content:
    content = content.replace(old2, new2)
    changes += 1
    print('OK: added turn_pid init')
else:
    print('MISS 2: turn_pid init')
    idx = content.find('ctrl->turn_kp')
    if idx >= 0:
        print(repr(content[idx:idx+150]))

# === 3. Add PID_Reset before LINE_STATE_TURNING ===
old3 = '            ctrl->line_state = LINE_STATE_TURNING;\n        } else {\n            /* 低速前进 */\n            float adj_pwm = ctrl->adjust_speed_pwm;\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)adj_pwm);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)adj_pwm);\n\n            ctrl->line_left_pwm  = adj_pwm;\n            ctrl->line_right_pwm = adj_pwm;\n        }\n        break;\n    }\n\n    /* ---- 状态 3: 直角转弯 (陀螺仪 Yaw 闭环 + 循迹模块辅助对准) ---- */'
new3 = '            PID_Reset(&ctrl->turn_pid);\n            ctrl->line_state = LINE_STATE_TURNING;\n        } else {\n            /* 低速前进 */\n            float adj_pwm = ctrl->adjust_speed_pwm;\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)adj_pwm);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)adj_pwm);\n\n            ctrl->line_left_pwm  = adj_pwm;\n            ctrl->line_right_pwm = adj_pwm;\n        }\n        break;\n    }\n\n    /* ---- 状态 3: 直角转弯 (陀螺仪 Yaw PID 闭环 + 循迹模块辅助对准) ---- */'

if old3 in content:
    content = content.replace(old3, new3)
    changes += 1
    print('OK: added PID_Reset before TURNING')
else:
    print('MISS 3: PID_Reset before TURNING')
    idx = content.find('LINE_STATE_TURNING')
    if idx >= 0:
        print(repr(content[idx-50:idx+100]))

# === 4. Add PID_Reset before LINE_STATE_INITIAL_TURN ===
old4 = '            ctrl->line_state = LINE_STATE_INITIAL_TURN;\n        } else {\n            /* 低速前进 */\n            float adj_pwm = ctrl->adjust_speed_pwm;\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)adj_pwm);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)adj_pwm);\n\n            ctrl->line_left_pwm  = adj_pwm;\n            ctrl->line_right_pwm = adj_pwm;\n        }\n        break;\n    }\n\n    /* ---- 状态 6: 初始 90° 转弯 (对齐第一条线) ---- */'
new4 = '            PID_Reset(&ctrl->turn_pid);\n            ctrl->line_state = LINE_STATE_INITIAL_TURN;\n        } else {\n            /* 低速前进 */\n            float adj_pwm = ctrl->adjust_speed_pwm;\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)adj_pwm);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)adj_pwm);\n\n            ctrl->line_left_pwm  = adj_pwm;\n            ctrl->line_right_pwm = adj_pwm;\n        }\n        break;\n    }\n\n    /* ---- 状态 6: 初始 90° 转弯 (对齐第一条线) ---- */'

if old4 in content:
    content = content.replace(old4, new4)
    changes += 1
    print('OK: added PID_Reset before INITIAL_TURN')
else:
    print('MISS 4: PID_Reset before INITIAL_TURN')
    idx = content.find('LINE_STATE_INITIAL_TURN')
    if idx >= 0:
        print(repr(content[idx-50:idx+100]))

# === 5. Replace P-control with PID in TURNING state ===
old5 = '            /* P 控制: 左轮反向、右轮正向 → 逆时针转弯 */\n            float turn_out = ctrl->turn_kp * yaw_error;\n            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;\n            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;\n\n            /* 保证最小转弯 PWM，克服静摩擦 */\n            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;\n            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;\n\n            float pwm_l = -turn_out;\n            float pwm_r =  turn_out;\n\n            ctrl->line_left_pwm  = pwm_l;\n            ctrl->line_right_pwm = pwm_r;\n\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);\n        }\n        break;\n    }\n\n    /* ---- 状态 4: 一条边完成 ---- */'
new5 = '            /* PID 控制: 左轮反向、右轮正向 → 逆时针转弯 */\n            float turn_out = PID_Compute(&ctrl->turn_pid,\n                                         ctrl->turn_target_yaw,\n                                         current_yaw, 0.002f);\n            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;\n            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;\n\n            /* 保证最小转弯 PWM，克服静摩擦 */\n            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;\n            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;\n\n            float pwm_l = -turn_out;\n            float pwm_r =  turn_out;\n\n            ctrl->line_left_pwm  = pwm_l;\n            ctrl->line_right_pwm = pwm_r;\n\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);\n        }\n        break;\n    }\n\n    /* ---- 状态 4: 一条边完成 ---- */'

if old5 in content:
    content = content.replace(old5, new5)
    changes += 1
    print('OK: replaced P with PID in TURNING')
else:
    print('MISS 5: TURNING P-control')
    idx = content.find('P 控制:')
    if idx >= 0:
        print(repr(content[idx:idx+300]))

# === 6. Replace P-control with PID in INITIAL_TURN state ===
old6 = '            /* P 控制: 左轮反向、右轮正向 → 逆时针转弯 */\n            float turn_out = ctrl->turn_kp * yaw_error;\n            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;\n            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;\n\n            /* 保证最小转弯 PWM，克服静摩擦 */\n            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;\n            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;\n\n            float pwm_l = -turn_out;\n            float pwm_r =  turn_out;\n\n            ctrl->line_left_pwm  = pwm_l;\n            ctrl->line_right_pwm = pwm_r;\n\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);\n        }\n        break;\n    }'

new6 = '            /* PID 控制: 左轮反向、右轮正向 → 逆时针转弯 */\n            float turn_out = PID_Compute(&ctrl->turn_pid,\n                                         ctrl->turn_target_yaw,\n                                         current_yaw, 0.002f);\n            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;\n            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;\n\n            /* 保证最小转弯 PWM，克服静摩擦 */\n            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;\n            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;\n\n            float pwm_l = -turn_out;\n            float pwm_r =  turn_out;\n\n            ctrl->line_left_pwm  = pwm_l;\n            ctrl->line_right_pwm = pwm_r;\n\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);\n        }\n        break;\n    }'

if old6 in content:
    content = content.replace(old6, new6)
    changes += 1
    print('OK: replaced P with PID in INITIAL_TURN')
else:
    print('MISS 6: INITIAL_TURN P-control')
    # find second occurrence of P control
    idx = content.find('P 控制:')
    if idx >= 0:
        idx2 = content.find('P 控制:', idx + 1)
        if idx2 >= 0:
            print(repr(content[idx2:idx2+300]))

with open(path, 'w', encoding='utf-8', newline='') as f:
    f.write(content)

print(f'\nTotal: {changes}/6 changes applied')
