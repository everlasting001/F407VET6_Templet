import sys

# ==========================================
# 1. MoveControl.h: array[4]
# ==========================================
path_h = r'c:\Users\yinyi\Desktop\STM32_HAL_PROJECT\F407VET6\F407VET6_Templet\Framework\Inc\MoveControl.h'
with open(path_h, 'r', encoding='utf-8') as f:
    h = f.read()

old_h = '    float            adjust_distance_mm;    /**< 微调前进距离 (传感器到轮轴距离) */'
new_h = '    float            adjust_distance_mm[4]; /**< 每个路口的微调前进距离 (传感器到轮轴距离) */'
if old_h in h:
    h = h.replace(old_h, new_h)
    with open(path_h, 'w', encoding='utf-8', newline='') as f:
        f.write(h)
    print('OK: struct array[4]')
else:
    print(f'MISS h: {repr(h[h.find("adjust_distance"):h.find("adjust_distance")+80])}')

# ==========================================
# 2. MoveControl.c: all references
# ==========================================
path_c = r'c:\Users\yinyi\Desktop\STM32_HAL_PROJECT\F407VET6\F407VET6_Templet\Framework\Src\MoveControl.c'
with open(path_c, 'r', encoding='utf-8') as f:
    c = f.read()

changes = 0

# 2a. Init: loop set all 4
old = '    ctrl->adjust_distance_mm       = DEFAULT_ADJUST_DISTANCE_MM;'
new = '    for (uint8_t i = 0; i < 4; i++) {\n        ctrl->adjust_distance_mm[i] = DEFAULT_ADJUST_DISTANCE_MM;\n    }'
if old in c:
    c = c.replace(old, new)
    changes += 1
    print('OK: init loop')
else:
    print('MISS init')

# 2b. FORWARD_ADJUST: index by edge_count
old = '        if (dist >= ctrl->adjust_distance_mm) {\n            /* 微调完成 → 停车, 记录起始 Yaw, 开始转弯 */'
new = '        if (dist >= ctrl->adjust_distance_mm[ctrl->edge_count]) {\n            /* 微调完成 → 停车, 记录起始 Yaw, 开始转弯 */'
if old in c:
    c = c.replace(old, new)
    changes += 1
    print('OK: FORWARD_ADJUST indexed')
else:
    print('MISS FORWARD_ADJUST')

# 2c. INITIAL_ADJUST: index 0
old = '        if (dist >= ctrl->adjust_distance_mm) {\n            /* 微调完成 → 停车, 进入初始转弯 */'
new = '        if (dist >= ctrl->adjust_distance_mm[0]) {\n            /* 微调完成 → 停车, 进入初始转弯 */'
if old in c:
    c = c.replace(old, new)
    changes += 1
    print('OK: INITIAL_ADJUST index 0')
else:
    print('MISS INITIAL_ADJUST')

# 2d. MoveControl_SetLineTrackConfig: set all 4
old = '    ctrl->adjust_distance_mm     = adjust_mm;'
new = '    for (uint8_t i = 0; i < 4; i++) {\n        ctrl->adjust_distance_mm[i] = adjust_mm;\n    }'
if old in c:
    c = c.replace(old, new)
    changes += 1
    print('OK: SetLineTrackConfig loop')
else:
    print('MISS SetLineTrackConfig')

with open(path_c, 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print(f'MoveControl.c: {changes}/4')

# ==========================================
# 3. Task1_LineTrack.c: 4 macros + init
# ==========================================
path_t = r'c:\Users\yinyi\Desktop\STM32_HAL_PROJECT\F407VET6\F407VET6_Templet\Application\Src\TaskProgram\Task1_LineTrack.c'
with open(path_t, 'r', encoding='utf-8') as f:
    t = f.read()

# 3a. Replace single macro with 4
old = '/** @brief 路口微调前进距离 (mm) — 传感器安装位置到轮轴中心距离 */\n#define TASK1_ADJUST_DISTANCE_MM      40.0f'
new = '/** @brief 每个路口微调前进距离 (mm) — 传感器安装位置到轮轴中心距离 */\n#define TASK1_ADJUST_DISTANCE_MM_0    60.0f\n#define TASK1_ADJUST_DISTANCE_MM_1    60.0f\n#define TASK1_ADJUST_DISTANCE_MM_2    60.0f\n#define TASK1_ADJUST_DISTANCE_MM_3    60.0f'
if old in t:
    t = t.replace(old, new)
    print('OK: Task1 4 macros')
else:
    print(f'MISS Task1 macro: {repr(t[t.find("TASK1_ADJUST_DISTANCE"):t.find("TASK1_ADJUST_DISTANCE")+80])}')

# 3b. Update SetLineTrackConfig call - keep single param (sets all 4)
# No change needed - SetLineTrackConfig already sets all 4

# 3c. Add per-index setter calls after SetLineTrackConfig
old = '                                   TASK1_ADJUST_DISTANCE_MM);'
new = '                                   TASK1_ADJUST_DISTANCE_MM_0);'
if old in t:
    t = t.replace(old, new)
    print('OK: Task1 config updated')
else:
    print('MISS Task1 config')

# 3d. Add individual per-index adjustments
old = '    /* 3. 设置微调前进速度 */'
new = '    /* 3. 各路口微调距离独立配置 */\n    move_ctrl.adjust_distance_mm[1] = TASK1_ADJUST_DISTANCE_MM_1;\n    move_ctrl.adjust_distance_mm[2] = TASK1_ADJUST_DISTANCE_MM_2;\n    move_ctrl.adjust_distance_mm[3] = TASK1_ADJUST_DISTANCE_MM_3;\n\n    /* 4. 设置微调前进速度 */'
if old in t:
    t = t.replace(old, new)
    print('OK: Task1 per-index setters')
else:
    print('MISS Task1 setters')

with open(path_t, 'w', encoding='utf-8', newline='') as f:
    f.write(t)

print('Done')
