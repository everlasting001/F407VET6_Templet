# STM32F407 分层架构与状态机指南

**版本**: 1.0  
**日期**: 2026-04-25  
**目标**: 为电赛项目提供规范化的分层架构、任务调度和状态机实现

---

## 目录

1. [架构概览](#架构概览)
2. [核心框架](#核心框架)
3. [使用指南](#使用指南)
4. [设计模式](#设计模式)
5. [常见应用场景](#常见应用场景)
6. [集成检查清单](#集成检查清单)

---

## 架构概览

### 分层结构

```
┌────────────────────────────────────────────────┐
│  应用层 (Application Layer)                    │
│  - 用户业务逻辑                                │
│  - 使用状态机、任务调度器                      │
│  - 文件位置: Core/App/                         │
└────────────────────────────────────────────────┘
                      ↑
                      │
┌────────────────────────────────────────────────┐
│  设备驱动层 (Device Layer)                     │
│  - 具体设备驱动（电机、传感器等）              │
│  - 实现虚函数指针表模式                        │
│  - 文件位置: Core/Drivers/                     │
└────────────────────────────────────────────────┘
                      ↑
                      │
┌────────────────────────────────────────────────┐
│  框架层 (Framework Layer)                      │
│  - 任务调度器（周期性任务）                    │
│  - 状态机框架（有限状态机）                    │
│  - 系统滴答定时器（1ms）                       │
│  - 文件位置: Core/Framework/                   │
└────────────────────────────────────────────────┘
                      ↑
                      │
┌────────────────────────────────────────────────┐
│  STM32 HAL 层 (Abstraction Layer)              │
│  - CubeMX 生成的初始化代码                     │
│  - GPIO、时钟、中断配置                        │
│  - 文件位置: Core/Src/                         │
└────────────────────────────────────────────────┘
                      ↑
                      │
┌────────────────────────────────────────────────┐
│  驱动层 (Driver Layer)                         │
│  - STM32 HAL 库（厂商提供）                    │
│  - CMSIS 库                                    │
│  - 启动代码                                    │
│  - 文件位置: Drivers/                          │
└────────────────────────────────────────────────┘
```

**分层设计优势**：
- ✅ 清晰的职责划分
- ✅ 代码复用性强
- ✅ 易于测试和维护
- ✅ 支持多种设备驱动替换

---

## 核心框架

### 1. 系统滴答定时器 (system_tick)

**功能**：提供 1ms 的系统时间戳和中断触发

**初始化**：
```c
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    system_tick_init(1000);  // 1ms 滴答
    app_init();

    while (1) {
        app_main_loop();
    }
}
```

**在 SysTick_Handler 中**：
```c
void SysTick_Handler(void) {
    HAL_IncTick();
    system_tick_update();  // 驱动任务调度
}
```

**API**：
```c
uint32_t system_get_ticks(void);                // 获取系统 tick（ms）
uint32_t system_elapsed_ms(start, end);         // 计算经过时间
void system_tick_enable(void) / disable(void);  // 启用/禁用中断
```

### 2. 任务调度器 (task_scheduler)

**功能**：基于 1ms 滴答的周期性任务调度，不依赖 RTOS

**工作原理**（手动分频）：
```
每次 scheduler_tick() 调用（每 1ms）：
  For each task:
    if (enabled):
      tick_count++
      if (tick_count >= period_ms):
        tick_count = 0
        task_func()  // 执行任务
```

**使用示例**：
```c
void app_init(void) {
    // 注册任务（周期单位：毫秒）
    scheduler_add_task(read_sensors, 10);       // 10ms 执行一次
    scheduler_add_task(motor_update, 5);        // 5ms 执行一次
    scheduler_add_task(control_loop, 50);       // 50ms 执行一次
}

void read_sensors(void) {
    // 读取传感器数据
    imu_read(&imu_data);
    grayscale_read(&gray_data);
}

void motor_update(void) {
    // 更新电机状态、检查过流等
    if (motor.current > THRESHOLD) {
        motor_stop();
    }
}

void control_loop(void) {
    // 运行控制算法
    compute_pid(&error, &correction);
    apply_correction(correction);
}
```

**API**：
```c
void scheduler_init(void);
int scheduler_add_task(task_func_t func, uint16_t period_ms);
void scheduler_remove_task(int task_id);
void scheduler_enable_task(int task_id);
void scheduler_disable_task(int task_id);
void scheduler_tick(void);  // 由 system_tick_update() 调用
```

**任务周期选择建议**：
| 任务类型 | 推荐周期 | 说明 |
|---------|--------|------|
| 电机控制 | 5ms | 快速响应 |
| 传感器读取 | 10-20ms | 平衡性能和功耗 |
| 控制算法 | 50ms | 典型的闭环控制周期 |
| 状态机更新 | 50ms | 避免频繁切换 |
| 通信处理 | 20ms | 足够快的消息处理 |

### 3. 状态机框架 (state_machine)

**功能**：通用的有限状态机（FSM），支持状态转移和回调

**三段模式**：
```
enter() → 进入状态时调用一次（初始化状态资源）
process() → 每次更新时调用（执行状态逻辑）
exit() → 离开状态时调用一次（清理资源）
```

**设计示例 - 循迹小车状态机**：
```c
// 1. 定义状态枚举
typedef enum {
    STATE_INIT,       // 初始化
    STATE_READY,      // 就绪
    STATE_RUNNING,    // 运行中
    STATE_ERROR,      // 错误
    STATE_STOPPED,    // 已停止
} RobotState_e;

// 2. 定义状态处理器
static void state_init_enter(void) {
    printf("State: INIT\r\n");
    motor_stop();
    sensor_init();
}

static void state_init_process(void) {
    if (system_get_ticks() > 100) {
        robot.fsm.next_state = STATE_READY;
    }
}

static void state_init_exit(void) {
    printf("Leaving INIT\r\n");
}

// 类似定义其他状态...

// 3. 创建状态处理器表
static const StateHandler_t g_robot_handlers[STATE_STOPPED + 1] = {
    [STATE_INIT] = {
        .enter = state_init_enter,
        .process = state_init_process,
        .exit = state_init_exit,
    },
    [STATE_READY] = {
        .enter = state_ready_enter,
        .process = state_ready_process,
        .exit = state_ready_exit,
    },
    [STATE_RUNNING] = {
        .enter = state_running_enter,
        .process = state_running_process,
        .exit = state_running_exit,
    },
    // ... 其他状态
};

// 4. 初始化状态机
typedef struct {
    StateMachine_t fsm;
    // ... 其他应用数据
} Robot_t;

Robot_t robot;

void robot_init(void) {
    state_machine_init(&robot.fsm,
                       g_robot_handlers,
                       5,  // 状态数
                       STATE_INIT);  // 初始状态
}

// 5. 在任务中更新状态机
static void robot_update_task(void) {
    state_machine_update(&robot.fsm);
}

void app_init(void) {
    robot_init();
    scheduler_add_task(robot_update_task, 50);
}
```

**API**：
```c
void state_machine_init(StateMachine_t *fsm,
                        const StateHandler_t *handlers,
                        uint8_t num_states,
                        uint8_t initial_state);
void state_machine_update(StateMachine_t *fsm);
void state_machine_set_state(StateMachine_t *fsm, uint8_t next_state);
uint8_t state_machine_get_state(const StateMachine_t *fsm);
uint8_t state_machine_get_next_state(const StateMachine_t *fsm);
```

### 4. 设备基类与虚函数指针 (device_base)

**功能**：定义所有设备驱动的通用接口，实现面向对象多态

**概念**：虚函数指针表（vtable）模式
```c
// 定义通用接口
typedef struct {
    void (*init)(void *dev);
    int (*read)(void *dev, void *buffer, uint32_t size);
    int (*write)(void *dev, const void *buffer, uint32_t size);
} DeviceOps_t;

// 具体实现提供操作表
typedef struct {
    DeviceOps_t ops;  // 虚函数表
    uint16_t data;    // 私有数据
} ConcreteDevice_t;

// 使用通用接口调用具体实现
dev->ops.init(dev);
dev->ops.read(dev, buffer, size);
```

**电机驱动示例**：
```c
// 定义电机特定的操作接口
typedef struct {
    int (*init)(void *dev);
    int (*set_speed)(void *dev, uint16_t speed);
    int (*set_direction)(void *dev, MotorDirection_e dir);
    int (*stop)(void *dev);
} MotorOps_t;

// 电机结构体包含虚函数表
typedef struct {
    MotorOps_t *ops;         // 虚函数表
    uint16_t pwm_value;      // 私有数据
    MotorDirection_e dir;
} Motor_t;

// 实现电机操作
static const MotorOps_t g_motor_ops = {
    .init = motor_init_impl,
    .set_speed = motor_set_speed_impl,
    .set_direction = motor_set_direction_impl,
    .stop = motor_stop_impl,
};

// 初始化时绑定操作表
Motor_t motor1, motor2;
motor_init(&motor1, &g_motor_ops, ...);
motor_init(&motor2, &g_motor_ops, ...);  // 可以是不同实现

// 使用时通过统一接口调用
motor1.ops->set_speed(&motor1, 500);
motor2.ops->set_speed(&motor2, 500);
```

**优势**：
- ✅ 支持多种实现替换
- ✅ 代码复用性强
- ✅ 易于单元测试
- ✅ 接近 OOP 特性

---

## 使用指南

### 步骤 1：项目初始化

```c
int main(void) {
    // 标准 HAL 初始化
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    // 框架初始化
    system_tick_init(1000);  // 1ms 滴答
    app_init();              // 应用初始化（注册任务、初始化状态机）

    // 主循环（所有工作由中断驱动）
    while (1) {
        __WFI();  // 进入睡眠，等待中断
    }
}
```

### 步骤 2：创建应用任务

```c
// 在 Core/App/Src/app_config.c 中

static void sensor_task(void) {
    // 这个函数每 10ms 执行一次
    imu_read(&imu_data);
    grayscale_read(&gray_data);
}

static void control_task(void) {
    // 这个函数每 50ms 执行一次
    float error = compute_line_error(&gray_data);
    float correction = pid_calculate(&controller, error);
    motor_apply_correction(correction);
}

void app_init(void) {
    // 初始化硬件
    motor_init(...);
    sensor_init(...);

    // 注册周期性任务
    scheduler_add_task(sensor_task, 10);
    scheduler_add_task(control_task, 50);

    // 初始化状态机
    robot_fsm_init();
}
```

### 步骤 3：创建设备驱动

```c
// 在 Core/Drivers/Inc/my_device.h 中

typedef struct {
    MyDeviceOps_t *ops;
    // ... 私有数据
} MyDevice_t;

// 在 Core/Drivers/Src/my_device.c 中

static int my_device_init_impl(MyDevice_t *dev) {
    // 初始化硬件
}

static const MyDeviceOps_t g_my_device_ops = {
    .init = (int (*)(void *))my_device_init_impl,
    // ... 其他操作
};

int my_device_init(MyDevice_t *dev, ...) {
    dev->ops = &g_my_device_ops;
    return my_device_init_impl(dev);
}
```

---

## 设计模式

### 1. 虚函数指针模式（多态）

**用途**：实现面向对象的多态性

**模板**：
```c
// 定义操作接口
typedef struct {
    int (*op1)(void *);
    int (*op2)(void *, int);
} ObjOps_t;

// 具体对象包含操作表
typedef struct {
    ObjOps_t *ops;
    int data;
} Obj_t;

// 调用时通过指针调用
obj->ops->op1(obj);
```

### 2. 手动分频计数器（任务调度）

**用途**：在中断中实现周期性任务分发，无需 RTOS

**模板**：
```c
typedef struct {
    void (*func)(void);
    uint16_t period_ms;    // 周期
    uint16_t tick_count;   // 计数器
} Task_t;

void scheduler_tick(void) {
    for (int i = 0; i < num_tasks; i++) {
        task->tick_count++;
        if (task->tick_count >= task->period_ms) {
            task->tick_count = 0;
            task->func();  // 执行任务
        }
    }
}
```

### 3. 状态机模式（有限状态机）

**用途**：管理复杂的工作流和状态转移

**三段处理**：
```
状态转移发生时：
  1. 调用旧状态的 exit()    - 清理资源
  2. 切换到新状态
  3. 调用新状态的 enter()   - 初始化资源

每次更新时：
  4. 调用当前状态的 process() - 执行逻辑
```

### 4. ISR 快速返回模式（中断安全）

**原则**：中断处理函数应快速返回，不执行长操作

**模式**：
```c
// ❌ 错误：在 ISR 中执行长操作
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) {
    HAL_Delay(100);            // 不能在 ISR 中
    process_complex_data();     // ISR 应该快速返回
    send_response();            // 可能导致死锁
}

// ✅ 正确：ISR 中仅设置标志
volatile uint8_t data_ready = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) {
    data_ready = 1;  // 快速返回
}

// 在主程序/任务中处理
static void uart_process_task(void) {
    if (data_ready) {
        data_ready = 0;
        process_complex_data();  // 在任务中进行
        send_response();
    }
}
```

---

## 常见应用场景

### 场景 1：循迹小车

**状态机**：
```
INIT (初始化) → READY (就绪) → RUNNING (循迹)
                    ↓
                  ERROR (错误)
                    ↓
                  STOPPED (停止)
```

**任务分配**：
```
灰度传感器读取       10ms
电机控制             5ms
循迹算法             50ms
状态机更新           50ms
```

**代码框架**：
```c
typedef enum {
    STATE_INIT = 0,
    STATE_READY,
    STATE_RUNNING,
    STATE_ERROR,
    STATE_STOPPED,
} LineTrackerState_e;

static StateMachine_t g_line_tracker_fsm;

void line_tracker_init(void) {
    state_machine_init(&g_line_tracker_fsm,
                       g_line_tracker_handlers,
                       5,
                       STATE_INIT);

    scheduler_add_task(read_grayscale, 10);
    scheduler_add_task(motor_control, 5);
    scheduler_add_task(line_tracking_algorithm, 50);
    scheduler_add_task(fsm_update_task, 50);
}

static void fsm_update_task(void) {
    state_machine_update(&g_line_tracker_fsm);
}
```

### 场景 2：避障机器人

**状态机**：
```
INIT → EXPLORE (探索) → OBSTACLE (障碍) → BACKUP (后退)
           ↓
         STOP (停止)
```

**任务分配**：
```
超声波测距           20ms
躲避算法             50ms
电机控制             5ms
```

### 场景 3：多设备协调

**使用虚函数指针统一控制多个设备**：
```c
Motor_t motor_left, motor_right;
Sensor_t imu;
Servo_t servo;

// 统一初始化接口
motor_init(&motor_left, ...);
motor_init(&motor_right, ...);
imu_init(&imu);
servo_init(&servo);

// 通过统一接口控制
motor_left.ops->set_speed(&motor_left, 500);
motor_right.ops->set_speed(&motor_right, 500);
```

---

## 集成检查清单

### 编译检查
- [ ] `cmake --preset Debug` 编译通过
- [ ] 无编译错误
- [ ] 无未使用变量警告（或已审查）

### 功能检查
- [ ] SysTick 中断每 1ms 触发一次
- [ ] 任务按指定周期执行（用示波器或 GDB 验证）
- [ ] 状态机状态转移正确
- [ ] 没有任务阻塞（所有任务快速返回）

### 代码规范
- [ ] 遵循 CLAUDE.md 中的嵌入式开发规范
- [ ] 中断处理函数快速返回
- [ ] 设备驱动使用虚函数指针模式
- [ ] 应用层使用任务调度器 + 状态机

### 性能检查
- [ ] CPU 占用率合理（< 70% 运行时间）
- [ ] 没有栈溢出
- [ ] 没有内存泄漏（无动态分配或已审计）
- [ ] 响应延迟在可接受范围内

---

## 快速参考

### 注册一个 10ms 任务
```c
scheduler_add_task(my_task, 10);
```

### 创建状态机
```c
static const StateHandler_t handlers[NUM_STATES] = { ... };
StateMachine_t fsm;
state_machine_init(&fsm, handlers, NUM_STATES, INITIAL_STATE);
state_machine_update(&fsm);
```

### 创建设备驱动
```c
typedef struct {
    DeviceOps_t *ops;
    int data;
} MyDevice_t;

MyDevice_t dev;
dev.ops->init(&dev);
dev.ops->read(&dev, buffer, size);
```

### 获取系统时间
```c
uint32_t t1 = system_get_ticks();
// ... 做一些工作
uint32_t elapsed = system_elapsed_ms(t1, system_get_ticks());
```

---

## 文件位置速查表

| 组件 | 头文件 | 源文件 |
|------|--------|--------|
| 系统滴答 | `Core/Framework/Inc/system_tick.h` | `Core/Framework/Src/system_tick.c` |
| 任务调度 | `Core/Framework/Inc/task_scheduler.h` | `Core/Framework/Src/task_scheduler.c` |
| 状态机 | `Core/Framework/Inc/state_machine.h` | `Core/Framework/Src/state_machine.c` |
| 设备基类 | `Core/Framework/Inc/device_base.h` | - |
| 电机驱动 | `Core/Drivers/Inc/motor_driver.h` | `Core/Drivers/Src/motor_driver.c` |
| 应用配置 | `Core/App/Inc/app_config.h` | `Core/App/Src/app_config.c` |
| 主程序 | - | `Core/Src/main.c` |
| 中断处理 | - | `Core/Src/stm32f4xx_it.c` |

---

## 常见问题

**Q: 为什么使用虚函数指针而不是 C++ 继承？**  
A: 嵌入式 C 不支持 C++，虚函数指针是实现多态的标准方式，代码体积小，性能好。

**Q: 任务周期可以动态调整吗？**  
A: 可以，但需要小心。建议在任务中检查一个外部变量来决定是否执行，而不是在调度器中修改周期。

**Q: 状态机支持嵌套状态吗？**  
A: 当前框架不支持，但可以在应用层实现（每个状态内部维护一个子状态机）。

**Q: 如何处理中断中的长操作？**  
A: 中断中仅设置标志，使用任务调度器在合适的时间处理。参见"ISR 快速返回模式"。

---

## 相关文档

- `CLAUDE.md` - 项目总体指南
- `Core/Framework/Inc/*.h` - 框架 API 文档
- `Core/Drivers/Inc/*.h` - 设备驱动接口文档

---

**创建日期**: 2026-04-25  
**最后更新**: 2026-04-25  
**维护者**: 电赛项目团队
