import sys

# === MoveControl.h: Add turn_start_tick and turn_ramp_ms ===
path_h = r'c:\Users\yinyi\Desktop\STM32_HAL_PROJECT\F407VET6\F407VET6_Templet\Framework\Inc\MoveControl.h'
with open(path_h, 'r', encoding='utf-8') as f:
    h = f.read()

old_h = '    float            turn_kp;           /**< 转弯 P 增益 (PWM/°) */'
new_h = '    uint32_t         turn_start_tick;   /**< 转弯起始时刻 (ms) */\n    float            turn_ramp_ms;      /**< 转弯加速斜坡时间 (ms, 默认 150) */\n    float            turn_kp;           /**< 转弯 P 增益 (PWM/°) */'
if old_h in h:
    h = h.replace(old_h, new_h)
    with open(path_h, 'w', encoding='utf-8', newline='') as f:
        f.write(h)
    print('OK: MoveControl.h updated')
else:
    print('MISS h')

# === MoveControl.c: All changes ===
path_c = r'c:\Users\yinyi\Desktop\STM32_HAL_PROJECT\F407VET6\F407VET6_Templet\Framework\Src\MoveControl.c'
with open(path_c, 'r', encoding='utf-8') as f:
    c = f.read()

changes = 0

# 1. Add DEFAULT_TURN_RAMP_MS after turn PID defines
old1 = '#define DEFAULT_TURN_INTEGRAL_LIM       200.0f  /**< 转弯积分限幅 */\n#define DEFAULT_ADJUST_DISTANCE_MM      80.0f   /**< 微调前进距离 (传感器到轮轴) */'
new1 = '#define DEFAULT_TURN_INTEGRAL_LIM       200.0f  /**< 转弯积分限幅 */\n#define DEFAULT_TURN_RAMP_MS            150.0f  /**< 转弯加速斜坡时间 (ms) */\n#define DEFAULT_ADJUST_DISTANCE_MM      80.0f   /**< 微调前进距离 (传感器到轮轴) */'
if old1 in c:
    c = c.replace(old1, new1)
    changes += 1
    print('OK: added DEFAULT_TURN_RAMP_MS')
else:
    print('MISS 1')

# 2. Init turn_ramp_ms and turn_start_tick
old2 = '    ctrl->turn_kp                  = DEFAULT_TURN_KP;\n    PID_Init(&ctrl->turn_pid, DEFAULT_TURN_KP, DEFAULT_TURN_KI, DEFAULT_TURN_KD,\n             DEFAULT_TURN_INTEGRAL_LIM, DEFAULT_TURN_PWM);'
new2 = '    ctrl->turn_kp                  = DEFAULT_TURN_KP;\n    ctrl->turn_ramp_ms             = DEFAULT_TURN_RAMP_MS;\n    ctrl->turn_start_tick          = 0;\n    PID_Init(&ctrl->turn_pid, DEFAULT_TURN_KP, DEFAULT_TURN_KI, DEFAULT_TURN_KD,\n             DEFAULT_TURN_INTEGRAL_LIM, DEFAULT_TURN_PWM);'
if old2 in c:
    c = c.replace(old2, new2)
    changes += 1
    print('OK: init turn_ramp and turn_start_tick')
else:
    print('MISS 2')

# 3. Set turn_start_tick at TURNING entry
old3 = '            PID_Reset(&ctrl->turn_pid);\n            ctrl->line_state = LINE_STATE_TURNING;'
new3 = '            PID_Reset(&ctrl->turn_pid);\n            ctrl->turn_start_tick = HAL_GetTick();\n            ctrl->line_state = LINE_STATE_TURNING;'
if old3 in c:
    c = c.replace(old3, new3)
    changes += 1
    print('OK: set turn_start_tick at TURNING entry')
else:
    print('MISS 3')

# 4. Set turn_start_tick at INITIAL_TURN entry
old4 = '            PID_Reset(&ctrl->turn_pid);\n            ctrl->line_state = LINE_STATE_INITIAL_TURN;'
new4 = '            PID_Reset(&ctrl->turn_pid);\n            ctrl->turn_start_tick = HAL_GetTick();\n            ctrl->line_state = LINE_STATE_INITIAL_TURN;'
if old4 in c:
    c = c.replace(old4, new4)
    changes += 1
    print('OK: set turn_start_tick at INITIAL_TURN entry')
else:
    print('MISS 4')

# 5. Apply ramp in TURNING state
old5 = '''            /* PID 控制: 左轮反向、右轮正向 → 逆时针转弯 */
            float turn_out = PID_Compute(&ctrl->turn_pid,
                                         ctrl->turn_target_yaw,
                                         current_yaw, 0.002f);
            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;
            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;

            /* 保证最小转弯 PWM，克服静摩擦 */
            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;'''

new5 = '''            /* PID 控制: 左轮反向、右轮正向 → 逆时针转弯 */
            float turn_out = PID_Compute(&ctrl->turn_pid,
                                         ctrl->turn_target_yaw,
                                         current_yaw, 0.002f);

            /* 梯形加速: 前 turn_ramp_ms 线性斜坡 */
            float elapsed_ms = (float)(HAL_GetTick() - ctrl->turn_start_tick);
            if (elapsed_ms < ctrl->turn_ramp_ms) {
                turn_out *= elapsed_ms / ctrl->turn_ramp_ms;
            }

            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;
            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;

            /* 保证最小转弯 PWM，克服静摩擦 */
            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;'''

if old5 in c:
    c = c.replace(old5, new5)
    changes += 1
    print('OK: applied ramp in TURNING')
else:
    print('MISS 5')

# 6. Apply ramp in INITIAL_TURN state
old6 = '''            /* PID 控制: 左轮反向、右轮正向 → 逆时针转弯 */
            float turn_out = PID_Compute(&ctrl->turn_pid,
                                         ctrl->turn_target_yaw,
                                         current_yaw, 0.002f);
            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;
            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;

            /* 保证最小转弯 PWM，克服静摩擦 */
            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;\n            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;\n\n            float pwm_l = -turn_out;\n            float pwm_r =  turn_out;\n\n            ctrl->line_left_pwm  = pwm_l;\n            ctrl->line_right_pwm = pwm_r;\n\n            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);\n            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);\n        }\n        break;\n    }'''

new6 = '''            /* PID 控制: 左轮反向、右轮正向 → 逆时针转弯 */
            float turn_out = PID_Compute(&ctrl->turn_pid,
                                         ctrl->turn_target_yaw,
                                         current_yaw, 0.002f);

            /* 梯形加速: 前 turn_ramp_ms 线性斜坡 */
            float elapsed_ms = (float)(HAL_GetTick() - ctrl->turn_start_tick);
            if (elapsed_ms < ctrl->turn_ramp_ms) {
                turn_out *= elapsed_ms / ctrl->turn_ramp_ms;
            }

            if (turn_out >  ctrl->turn_pwm) turn_out =  ctrl->turn_pwm;
            if (turn_out < -ctrl->turn_pwm) turn_out = -ctrl->turn_pwm;

            /* 保证最小转弯 PWM，克服静摩擦 */
            if (turn_out > 0.0f && turn_out < 150.0f) turn_out = 150.0f;
            if (turn_out < 0.0f && turn_out > -150.0f) turn_out = -150.0f;

            float pwm_l = -turn_out;
            float pwm_r =  turn_out;

            ctrl->line_left_pwm  = pwm_l;
            ctrl->line_right_pwm = pwm_r;

            DCMotor_SetSpeed(ctrl->motor_left,  (int16_t)pwm_l);
            DCMotor_SetSpeed(ctrl->motor_right, (int16_t)pwm_r);
        }
        break;
    }'''

if old6 in c:
    c = c.replace(old6, new6)
    changes += 1
    print('OK: applied ramp in INITIAL_TURN')
else:
    print('MISS 6')

with open(path_c, 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print(f'Total: {changes}/6 changes')
