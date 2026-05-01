# PID 控制算法与运动控制 — 使用指南

## 目录

1. [模块概览](#1-模块概览)
2. [PID 控制算法](#2-pid-控制算法)
3. [MoveControl 运动控制](#3-movecontrol-运动控制)
4. [快速上手](#4-快速上手)
5. [PID 调参指南](#5-pid-调参指南)
6. [Vofa 在线调参](#6-vofa-在线调参)

---

## 1. 模块概览

| 文件 | 位置 | 职责 |
|------|------|------|
| `PID.h` / `PID.c` | `Devices/Inc/FilterAlgorithm/` `Devices/Src/FilterAlgorithm/` | PID 控制算法 (位置式 + 增量式) |
| `MoveControl.h` / `MoveControl.c` | `Framework/Inc/` `Framework/Src/` | 小车直行级联控制 (位置环 + 速度环 + 差速修正) |

**数据流:**

```
目标距离/速度 ──→ [MoveControl] ──→ DCMotor (执行)
                       ↑
                  Encoder (反馈)
```

---

## 2. PID 控制算法

### 2.1 数据结构

`PID_Controller_t` 封装了 PID 算法的全部参数和状态:

```c
typedef struct {
    float Kp, Ki, Kd;           // 核心参数 (运行时可调)
    float target, feedback;     // 目标值 / 反馈值
    float error, last_error;    // 误差状态
    float integral;             // 积分累积
    float output;               // 控制输出
    float output_max, output_min;   // 输出限幅
    float deadzone;             // 输入死区
    PID_Type_e type;            // 位置式 / 增量式
    // ... 增强功能使能标志
} PID_Controller_t;
```

### 2.2 两种模式

| 特性 | 位置式 PID | 增量式 PID |
|------|-----------|-----------|
| 公式 | `u = Kp*e + Ki*∫e + Kd*(Δe)` | `Δu = Kp*(e0-e1) + Ki*e0 + Kd*(e0-2e1+e2)` |
| 输出 | 绝对值 (与执行器位置对应) | 增量 (累加到上次输出) |
| 积分 | 需手动管理积分饱和 | 天然抗饱和 |
| 适用 | 位置控制、速度控制 | 需要无扰切换的场景 |
| 本项目 | **推荐用于位置环和速度环** | 备用 |

### 2.3 增强功能

所有增强功能通过使能标志独立控制:

| 功能 | 标志 | 说明 |
|------|------|------|
| 积分分离 | `enable_integral_separation` | `|error| < threshold` 时才积分, 防止大偏差时积分饱和 |
| 积分限幅 | `enable_integral_limit` | 钳位积分到 `[integral_min, integral_max]` |
| 抗积分饱和 | `enable_anti_windup` | 输出饱和时衰减积分 (×0.95), 防止深度饱和 |
| 输出限幅 | `enable_output_limit` | 钳位输出到 `[output_min, output_max]` |
| 输入死区 | `enable_deadzone` | `|error| < deadzone` 时输出为零, 避免小误差抖动 |
| 微分先行 | `enable_derivative_primer` | 对反馈值微分 (非误差), 防止目标突变时的微分冲击 |
| 微分滤波 | `enable_derivative_filter` | 一阶低通滤波: `y = α*x + (1-α)*y_last`, α 越小滤波越强 |

### 2.4 API 速查

```c
/* 初始化 */
void PID_Init(PID_Controller_t *pid, PID_Type_e type);
void PID_Reset(PID_Controller_t *pid);

/* 核心计算 */
float PID_Calculate(float target, float feedback, PID_Controller_t *pid);

/* 参数设置 (Kp/Ki/Kd 均为运行时可调) */
void PID_SetParams(PID_Controller_t *pid, float kp, float ki, float kd);
void PID_SetOutputLimit(PID_Controller_t *pid, float max, float min);
void PID_SetIntegralLimit(PID_Controller_t *pid, float max, float min);
void PID_SetDeadzone(PID_Controller_t *pid, float deadzone);
void PID_SetIntegralSeparation(PID_Controller_t *pid, float threshold);

/* 状态查询 */
float PID_GetOutput(const PID_Controller_t *pid);
float PID_GetError(const PID_Controller_t *pid);
```

---

## 3. MoveControl 运动控制

### 3.1 控制模式

| 模式 | 枚举 | 说明 |
|------|------|------|
| 停止 | `MOVE_MODE_STOP` | 电机断电, PID 复位 |
| 定位置 | `MOVE_MODE_POSITION` | 前进/后退指定距离 |
| 定速 | `MOVE_MODE_SPEED` | 以恒定速度行驶 |

### 3.2 位控模式 — 级联控制

```
位置外环                  速度内环                  执行
                                                
target_dist ──→ [位置PID] ──→ base_vel ──┬──→ [速度PID_L] ──→ PWM_L → 左电机
                 (pid_pos)               │                                    
                                         ├──→ [速度PID_R] ──→ PWM_R → 右电机
                   差速修正 ←────────────┘                                    
              turn_correction = dist_diff*Kp + d(dist_diff)*Kd                
```

**差速修正**: 若右轮走得多 (dist_diff > 0), 说明车右偏, 则:
- 左轮目标速度 += turn_correction (加速补偿)
- 右轮目标速度 -= turn_correction (减速修正)

### 3.3 速控模式

```
target_speed ──┬──→ [速度PID_L] ──→ PWM_L → 左电机
               ├──→ [速度PID_R] ──→ PWM_R → 右电机
               └──→ 差速修正 (基于位置漂移)
```

### 3.4 数据结构

```c
typedef struct {
    Encoder_t  *enc_left, *enc_right;   // 编码器 (反馈)
    DCMotor_t  *motor_left, *motor_right; // 电机 (执行)
    
    PID_Controller_t pid_pos;           // 位置环 PID
    PID_Controller_t pid_vel_l;         // 左轮速度环 PID
    PID_Controller_t pid_vel_r;         // 右轮速度环 PID
    
    float balance_kp, balance_kd;       // 差速修正增益
    MoveMode_e control_mode;            // 控制模式
    float target_distance_mm;           // 目标距离
    float target_speed_mmps;            // 目标速度
    
    int16_t pwm_max;                    // PWM 上限 (默认 50)
    int16_t pwm_min_run;                // PWM 最低运行值 (默认 10)
} MoveControl_t;
```

### 3.5 API 速查

```c
/* 初始化 */
void MoveControl_Init(MoveControl_t *mc,
                      Encoder_t *enc_l, Encoder_t *enc_r,
                      DCMotor_t *mot_l, DCMotor_t *mot_r);

/* 控制更新 (25Hz, 主循环调用) */
void MoveControl_Update(MoveControl_t *mc);

/* 模式设置 */
void MoveControl_SetDistanceTarget(MoveControl_t *mc, float distance_mm);
void MoveControl_SetSpeedTarget(MoveControl_t *mc, float speed_mmps);
void MoveControl_Stop(MoveControl_t *mc);

/* PID 参数 (Vofa 调参入口) */
void MoveControl_SetPosPID(MoveControl_t *mc, float kp, float ki, float kd);
void MoveControl_SetVelPID(MoveControl_t *mc, float kp, float ki, float kd);

/* 状态查询 */
uint8_t MoveControl_HasArrived(const MoveControl_t *mc);
float   MoveControl_GetAvgDistance(const MoveControl_t *mc);
float   MoveControl_GetAvgSpeed(const MoveControl_t *mc);
```

---

## 4. 快速上手

### 4.1 最小示例

```c
#include "MoveControl.h"

/* 全局运动控制器 (静态分配, 无 malloc) */
static MoveControl_t move_ctrl;

int main(void) {
    /* HAL 初始化... */

    /* 1. 初始化运动控制器 (绑定编码器和电机) */
    MoveControl_Init(&move_ctrl,
                     &left_encoder, &right_encoder,
                     &left_motor,   &right_motor);

    /* 2. 设置 PID 参数 (初始参考值, 后续 Vofa 调优) */
    MoveControl_SetPosPID(&move_ctrl,  0.17f, 0.10f, 0.10f);
    MoveControl_SetVelPID(&move_ctrl,  0.15f, 0.10f, 0.02f);
    MoveControl_SetBalanceGain(&move_ctrl, 0.5f, 0.2f);

    /* 3. 清零编码器, 设目标 */
    Encoder_HardReset(move_ctrl.enc_left);
    Encoder_HardReset(move_ctrl.enc_right);
    MoveControl_SetDistanceTarget(&move_ctrl, 600.0f);  // 前进 600mm

    while (1) {
        /* 4. 主循环中 40ms 周期更新 */
        if (Flag_40ms) {
            Flag_40ms = 0;

            /* 更新编码器 (通过 SensorBase_Run) */
            SensorBase_Run((SensorBase_t *)move_ctrl.enc_left);
            SensorBase_Run((SensorBase_t *)move_ctrl.enc_right);

            /* 更新运动控制 */
            MoveControl_Update(&move_ctrl);

            /* 检查是否到达 */
            if (MoveControl_HasArrived(&move_ctrl)) {
                /* 到达目标, 执行下一段运动... */
            }
        }
    }
}
```

### 4.2 多段运动

```c
typedef enum { SEG_WAIT, SEG_GO, SEG_DONE } SegState_e;

static SegState_e seg = SEG_WAIT;
static uint32_t seg_timer = 0;

/* 在主循环中: */
if (seg == SEG_WAIT && key_pressed) {
    Encoder_HardReset(move_ctrl.enc_left);
    Encoder_HardReset(move_ctrl.enc_right);
    MoveControl_SetDistanceTarget(&move_ctrl, 600.0f);
    seg = SEG_GO;
}
if (seg == SEG_GO && MoveControl_HasArrived(&move_ctrl)) {
    seg = SEG_DONE;
}
```

---

## 5. PID 调参指南

### 5.1 经验法 (从 P 开始)

1. **P 项**: 从 0 开始增大, 直到系统对阶跃响应略有超调, 取该值的 60%~70%
2. **I 项**: 从 0 开始增大, 消除稳态误差, 但不要引入振荡
3. **D 项**: 从 0 开始增大, 抑制超调和振荡, 但噪声大时需配合微分滤波

### 5.2 默认参数 (参考值)

这些是编码器反馈 + TB6612 驱动 + 指定电机的经验值:

| PID 控制器 | Kp | Ki | Kd | 输出限幅 | 说明 |
|-----------|----|----|----|---------|------|
| 位置环 (pid_pos) | 0.17 | 0.10 | 0.10 | ±50 | 距离→速度 |
| 速度环 (pid_vel) | 0.15 | 0.10 | 0.02 | ±50 | 速度→PWM |
| 差速修正 (balance) | 0.5 | — | 0.2 | — | 保持直线 |

### 5.3 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| 到目标停不下来 (过冲) | Kp 太大 / Kd 太小 / 缺积分分离 | 减小 Kp, 增大 Kd, 开启积分分离 |
| 到不了目标 (静态误差) | Ki 太小 / 积分分离阈值太小 | 增大 Ki, 增大分离阈值 |
| 前进时跑偏 | balance_kp 太小 | 增大 balance_kp |
| 前进时蛇形摆动 | balance_kp 太大 / balance_kd 太小 | 减小 Kp, 增大 Kd |
| 低速抖动 | 死区太小 / PWM 分辨率不够 | 增大 deadzone, 提高 pwm_min_run |
| 积分深度饱和 (启动后猛冲) | 积分累积过多 | 开启 integral_limit + anti_windup |

---

## 6. Vofa 在线调参

### 6.1 设计理念

所有 `Kp/Ki/Kd` 均为 `PID_Controller_t` 结构体字段, **不是** `#define` 宏。
这允许在运行时通过串口接收新参数并实时更新 PID:

```c
/* Vofa/串口调参伪代码 */
void Vofa_OnReceiveParams(float kp_pos, float ki_pos, float kd_pos,
                          float kp_vel, float ki_vel, float kd_vel)
{
    MoveControl_SetPosPID(&move_ctrl, kp_pos, ki_pos, kd_pos);
    MoveControl_SetVelPID(&move_ctrl, kp_vel, ki_vel, kd_vel);
}
```

### 6.2 建议的串口协议

| 帧头 | 命令 | 数据 (float × 6) | 帧尾 |
|------|------|------------------|------|
| 0xAA | 0x01 | Kp_pos, Ki_pos, Kd_pos, Kp_vel, Ki_vel, Kd_vel | 0x55 |

或使用文本协议 (便于 Vofa 的 JustFloat 引擎):
```
<kp_pos>,<ki_pos>,<kd_pos>,<kp_vel>,<ki_vel>,<kd_vel>\n
```

### 6.3 调参流程

1. 将 PID 参数初始化为默认值
2. 通过 Vofa 发送新参数 (Kp/Ki/Kd) 到 STM32 串口
3. STM32 接收后调用 `MoveControl_SetPosPID()` / `MoveControl_SetVelPID()`
4. 观察编码器反馈波形 (Vofa 示波器), 调整参数
5. 最优参数确认后, 记录并写回代码中的默认值

### 6.4 调参用调试输出

建议在调参时将以下数据通过串口发送到 Vofa 显示:

```c
/* Vofa JustFloat 引擎数据格式 (小端 float): */
float debug_data[] = {
    target_distance_mm,         // 通道0: 目标距离
    MoveControl_GetAvgDistance(&move_ctrl),  // 通道1: 实际平均距离
    MoveControl_GetAvgSpeed(&move_ctrl),     // 通道2: 实际平均速度
    (float)MoveControl_GetPWMLeft(&move_ctrl),  // 通道3: 左轮PWM
    (float)MoveControl_GetPWMRight(&move_ctrl), // 通道4: 右轮PWM
    move_ctrl.pid_pos.error,    // 通道5: 位置误差
};

/* 在 40ms 周期内通过 DMA 发送 */
UartBase_TransmitDMA(&dbg_printf.uart, (uint8_t *)debug_data, sizeof(debug_data));
```

---

## 7. 文件清单

```
Devices/
├── Inc/FilterAlgorithm/
│   └── PID.h                          # PID 结构体与 API 声明
└── Src/FilterAlgorithm/
    └── PID.c                          # PID 算法实现

Framework/
├── Inc/
│   └── MoveControl.h                  # 运动控制 API 声明
└── Src/
    └── MoveControl.c                  # 级联控制实现

CMakeLists.txt                         # 已更新 (新增源文件和 include 路径)
```

## 8. 注意事项

- **控制周期**: 25Hz (40ms), 与 Encoder 更新同频。如需改变频率, 需同步调整 Encoder 更新周期和 PID 参数
- **PWM 限幅**: `pwm_max=50` 对应 DCMotor 的 `DCMOTOR_ARR_MAX=2099` 的约 2.4% 占空比 — 这是为了安全调试而设的保守值, 正式使用时可根据实际需要调大
- **无动态内存**: 所有结构体均为静态或栈分配, 符合嵌入式最佳实践
- **ISR 安全**: `MoveControl_Update()` 应在主循环中调用, 不在 ISR 中调用
- **FPU**: STM32F407 有硬件 FPU, PID 浮点运算开销很小
- **左/右独立 PID**: 左右轮速度环 PID 独立, 避免数据覆盖和意外耦合
