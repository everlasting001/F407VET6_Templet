# 🧱 面向对象编程指南（OOP Guide）

> **适用工程**: STM32F407VET6 电赛项目  
> **语言**: C（通过结构体 + 函数指针模拟面向对象）  
> **目标读者**: 有基础编程经验、需要了解本项目 OOP 惯例的开发者

---

## 📑 目录

- [1. 概述](#1-概述)
- [2. 四大基本特性](#2-四大基本特性)
  - [2.1 封装（Encapsulation）](#21-封装encapsulation)
  - [2.2 继承（Inheritance）](#22-继承inheritance)
  - [2.3 多态（Polymorphism）](#23-多态polymorphism)
  - [2.4 抽象（Abstraction）](#24-抽象abstraction)
- [3. 常见设计模式](#3-常见设计模式)
  - [3.1 单例模式（Singleton）](#31-单例模式singleton)
  - [3.2 工厂模式（Factory）](#32-工厂模式factory)
  - [3.3 观察者模式（Observer）](#33-观察者模式observer)
- [4. SOLID 原则](#4-solid-原则)
  - [4.1 单一职责原则（SRP）](#41-单一职责原则srp)
  - [4.2 开闭原则（OCP）](#42-开闭原则ocp)
  - [4.3 里氏替换原则（LSP）](#43-里氏替换原则lsp)
  - [4.4 接口隔离原则（ISP）](#44-接口隔离原则isp)
  - [4.5 依赖倒置原则（DIP）](#45-依赖倒置原则dip)
- [5. 推荐的组织方式](#5-推荐的组织方式)
  - [5.1 模块/命名空间划分](#51-模块命名空间划分)
  - [5.2 类（结构体）设计规范](#52-类结构体设计规范)
  - [5.3 接口设计惯例](#53-接口设计惯例)
  - [5.4 文件组织规则](#54-文件组织规则)
- [附录：快速参考](#附录快速参考)

---

## 1. 概述

本项目使用 **C 语言** 实现嵌入式系统，由于 C 语言原生不支持类、继承、多态等 OOP 语法，我们通过以下惯用技法模拟面向对象：

| OOP 概念 | C 语言实现方式 | 本项目示例 |
|----------|---------------|-----------|
| 类（Class） | `struct` 结构体 | [`PID_Controller_t`](Lib/01APP/Inc/Config.h:82) |
| 方法（Method） | 以类名前缀的普通函数 | `PID_Calculate()`, `Motor_SetPWM()` |
| 封装 | `static` + 头文件接口 | `.c` 文件内 `static` 函数 |
| 继承 | 基类结构体作为第一个成员 | [`ModuleBase_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h:78) |
| 多态 | 虚函数表（函数指针结构体） | [`ModuleVTable_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h:63) |
| 接口 | 头文件中的函数声明 | `*_Headers.h` 聚合头文件 |

> **原则**: 在资源受限的 MCU 上，避免 C++ 的 RTTI、异常等运行时开销，用纯 C 实现 OOP 的**设计思想**而非语法糖。

---

## 2. 四大基本特性

### 2.1 封装（Encapsulation）

**定义**: 将数据与操作数据的方法捆绑在一起，对外隐藏内部实现细节。

#### 在本项目中的体现

**示例 A：PID 控制器** — 数据与方法捆绑

[`PID_Controller_t`](Lib/01APP/Inc/Config.h:38) 结构体封装了所有 PID 相关的状态：

```c
// Config.h — 数据定义
typedef struct {
    float Kp, Ki, Kd;           // PID 参数
    float target, feedback;      // 控制目标与反馈
    float error, integral;       // 误差与积分
    float output;                // 计算输出
    // ... 更多内部状态
    uint8_t enable_anti_windup;  // 抗积分饱和使能
} PID_Controller_t;
```

操作函数集中在 [`pid.h`](Lib/03BSP/Inc/pid.h) 中声明：

```c
// pid.h — 公有接口（对外的"方法"）
void PID_Init(PID_Controller_t *pid, PID_Type_e type);
float PID_Calculate(float target, float feedback, PID_Controller_t *pid);
void PID_SetParams(PID_Controller_t *pid, float kp, float ki, float kd);
void PID_SetOutputLimit(PID_Controller_t *pid, float max_val, float min_val);
```

**示例 B：内部实现隐藏**

在 [`pid.c`](Lib/03BSP/Src/pid.c) 中，内部辅助函数用 `static` 隐藏：

```c
// pid.c — 外部不可见
static float PID_ApplyDeadzone(PID_Controller_t *pid, float error) {
    if (pid->enable_deadzone && fabsf(error) < pid->deadzone) {
        return 0.0f;  // 死区内归零
    }
    return error;
}
```

**✅ 实践规则**

- 结构体定义放在 `.h` 文件中，但**内部状态应有注释说明"自动计算，无需手动修改"**
- `.c` 文件内的 `static` 函数和变量对外部完全不可见
- 所有对结构体的操作**必须通过接口函数**，禁止直接修改内部成员

---

### 2.2 继承（Inheritance）

**定义**: 子类复用基类的数据和行为，并可以扩展或重写。

> 在 C 语言中通过 **"基类结构体作为子类第一个成员"** 实现，确保指针类型转换合法。

#### 在本项目中的体现

**示例：设备模块基类体系**

基类定义于 [`ModuleBase.h`](Devices/Inc/DeviceClass/Modules/ModuleBase.h)：

```c
// 基类结构体
typedef struct ModuleBase_s {
    const ModuleVTable_t *vtable;    // 虚函数表指针
    const char           *name;      // 模块名称
    uint8_t               initialized; // 初始化标志
} ModuleBase_t;
```

子类定义（假设 LED 模块）：

```c
// LED.h — 子类继承基类
typedef struct {
    ModuleBase_t base;       // 基类（必须是第一个成员！）
    GPIO_TypeDef *port;      // LED 所在 GPIO 端口
    uint16_t      pin;       // LED 所在 GPIO 引脚
    uint8_t       active_low;// 低电平有效标志
} LED_t;

// 使用基类指针操作子类对象（安全转换）
LED_t led;
ModuleBase_t *pBase = (ModuleBase_t *)&led;  // ✅ 合法转换
```

基类提供构造/析构函数：

```c
// 构造 — 设置默认 vtable 和名称
void ModuleBase_Constructor(ModuleBase_t *self, const char *name);

// 析构 — 自动调用 cleanup，清空指针防悬空
void ModuleBase_Destructor(ModuleBase_t *self);
```

**✅ 实践规则**

- 基类结构体**必须**是子类结构体的**第一个成员**（保证指针转换正确）
- 构造时首先调用 `ModuleBase_Constructor()`，然后替换 vtable
- 析构时调用 `ModuleBase_Destructor()`，会**自动执行 cleanup**
- 使用 `ModuleBase_IsInitialized()` 检查状态，避免重复初始化

---

### 2.3 多态（Polymorphism）

**定义**: 同一接口在不同子类上有不同实现。C 中通过**虚函数表（VTable）** 实现。

#### 在本项目中的体现

**示例：模块虚函数表**

[`ModuleVTable_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h:63) 定义了设备模块的统一接口：

```c
typedef struct {
    int (*init)(void *self);       // 初始化
    int (*run)(void *self);        // 运行主逻辑
    int (*cleanup)(void *self);    // 清理资源
    void (*reset)(void *self);     // 复位
} ModuleVTable_t;
```

**子类实现多态**（三步走）：

```c
// 第1步：实现子类虚函数
static int LED_init(void *self) {
    LED_t *led = (LED_t *)self;
    HAL_GPIO_WritePin(led->port, led->pin, led->active_low ? 1 : 0);
    return 0;
}

static int LED_run(void *self) {
    LED_t *led = (LED_t *)self;
    HAL_GPIO_TogglePin(led->port, led->pin);
    return 0;
}

// 第2步：定义子类虚函数表
static const ModuleVTable_t led_vtable = {
    .init    = LED_init,
    .run     = LED_run,
    .cleanup = NULL,       // 使用基类默认实现
    .reset   = NULL,
};

// 第3步：构造时替换 vtable
LED_t led;
ModuleBase_Constructor(&led.base, "LED");
led.base.vtable = &led_vtable;  // 替换为子类虚函数表
```

**多态调用**：

```c
// 通过基类指针统一调用
ModuleBase_t *modules[] = {
    (ModuleBase_t *)&led,
    (ModuleBase_t *)&key,
    (ModuleBase_t *)&oled,
};

for (int i = 0; i < 3; i++) {
    ModuleBase_Init(modules[i]);   // 实际调用各自的 init
    ModuleBase_Run(modules[i]);    // 实际调用各自的 run
}
```

**基类默认行为**：若子类未实现某个虚函数（指针为 NULL），基类提供空实现并静默忽略。

**✅ 实践规则**

- 虚函数表使用 `const` 修饰，存放在 Flash（只读区域），不占用 RAM
- 所有虚函数的第一个参数必须是 `void *self`
- 返回 `int`，0 表示成功，负数表示错误码
- 若子类不需要某个虚函数，置为 `NULL` 即可使用基类默认实现

---

### 2.4 抽象（Abstraction）

**定义**: 隐藏复杂实现，只暴露必要接口。使用者只需关心"能做什么"，而非"怎么做的"。

#### 在本项目中的体现

**示例：分层抽象**

本项目采用清晰的分层架构，每层只对上层的特定范围暴露接口：

```
┌─────────────────────────────────┐
│  01APP（应用层）                  │
│  Task.c, CallBack.c, Variable.c  │  ← 只关心业务逻辑
├─────────────────────────────────┤
│  02COM（控制/通信层）             │
│  Move_Control, State_Machine     │  ← 只关心控制策略
├─────────────────────────────────┤
│  03BSP（板级支持包）              │
│  Motor, Encoder, PID, Serial    │  ← 只关心硬件操作
├─────────────────────────────────┤
│  Devices/（设备抽象层）           │
│  ModuleBase 虚函数表体系          │  ← 定义设备接口规范
├─────────────────────────────────┤
│  Core/（框架层 + HAL）           │
│  HAL 库, 任务调度器, 状态机框架   │  ← 底层基础设施
└─────────────────────────────────┘
```

**接口抽象示例**：电机控制

[`Motor.h`](Lib/03BSP/Inc/Motor.h) 对外只暴露 4 个函数：

```c
void Motor_Init(void);
void Motor_SetPWM(int16_t PWM, MotorIndex_e motor);
void Motor_Stop(MotorIndex_e motor);
void Spin_Left(uint16_t PWM);
void Spin_Right(uint16_t PWM);
```

使用者**无需知道**电机是直流有刷还是无刷、PWM 通道号、GPIO 引脚等细节。

**✅ 实践规则**

- 头文件只放调用者需要的声明，内部宏/静态函数放在 `.c` 中
- 使用聚合头文件（如 [`BSP_Headers.h`](Lib/03BSP/Inc/BSP_Headers.h)）对上层隐藏下层模块的独立头文件细节
- 返回值用 `int`，0 成功 / 负值错误，错误码应在头文件中用 `#define` 或 `enum` 定义

---

## 3. 常见设计模式

### 3.1 单例模式（Singleton）

**定义**: 确保一个类只有一个实例，并提供全局访问点。

#### 适用场景

- **硬件外设管理器**：UART、I2C、SPI 等外设在 MCU 上只有一套硬件寄存器
- **全局状态对象**：如小车姿态 [`pose_t`](Lib/01APP/Inc/Config.h:95)、系统配置
- **调度器核心**：任务调度器 [`task_scheduler`](Core/Src/task_scheduler.c) 在系统中只有一份

#### 实现建议

```c
// pose_manager.h — 单例接口
typedef struct {
    float x, y, yaw;
    uint8_t initialized;
} PoseManager_t;

// 全局访问点 — 返回指向唯一实例的指针
PoseManager_t *PoseManager_GetInstance(void);

// 使用示例
PoseManager_t *pose = PoseManager_GetInstance();
pose->yaw = 45.0f;
```

```c
// pose_manager.c — 实现
static PoseManager_t s_instance = {0};  // 唯一实例（静态全局）

PoseManager_t *PoseManager_GetInstance(void) {
    if (!s_instance.initialized) {
        s_instance.x = 0.0f;
        s_instance.y = 0.0f;
        s_instance.yaw = 0.0f;
        s_instance.initialized = 1;
    }
    return &s_instance;
}
```

> **本项目现状**: 全局变量已在 [`Variable.h`](Lib/01APP/Inc/Variable.h) 中用 `extern` 声明，本质上就是单例。推荐重构为上述带 `GetInstance()` 的封装形式，以获得更好的封装性和延迟初始化能力。

**✅ 最佳实践**

- 使用 `static` 全局变量保存唯一实例，文件作用域防止外部直接访问
- 提供 `*_GetInstance()` 或 `*_GetHandle()` 作为访问点
- 嵌入式环境可省略懒加载（直接编译期初始化），避免首次调用时的判断开销
- **禁止**在中断服务函数（ISR）中调用可能触发首次初始化的单例访问

---

### 3.2 工厂模式（Factory）

**定义**: 将对象的创建逻辑封装在专门的函数中，调用者不需要知道具体类型。

#### 适用场景

- **多型号硬件**：通过编译宏选择不同电机型号（JGA370 vs MG310）
- **模块动态创建**：根据配置创建设备对象（LED/KEY/OLED 等）
- **传感器抽象**：不同类型的传感器（模拟灰度、数字 IMU）提供统一的读数接口

#### 本项目的现有工厂

[`Config.h`](Lib/01APP/Inc/Config.h:120) 已通过预处理宏实现最简单的"工厂"：

```c
// Config.h — 编译期工厂
//#define MOTOR_TYPE_JGA370   // 选择JGA-370
#define MOTOR_TYPE_MG310      // 选择MG-310

#ifdef MOTOR_TYPE_JGA370
    #define ENCODER_LINE 11
    #define GEAR_RATIO   9.6f
#elif defined(MOTOR_TYPE_MG310)
    #define ENCODER_LINE 13
    #define GEAR_RATIO   20.409f
#endif
```

#### 改进建议：运行期工厂

```c
// motor_factory.h
typedef struct {
    void   (*init)(void);
    void   (*set_speed)(int16_t pwm);
    int16_t (*get_rpm)(void);
} MotorInterface_t;

// 工厂函数 — 根据参数返回不同实现
const MotorInterface_t *MotorFactory_GetMotor(MotorIndex_e index);
```

```c
// motor_factory.c
static const MotorInterface_t left_motor = {
    .init      = LeftMotor_Init,
    .set_speed = LeftMotor_SetSpeed,
    .get_rpm   = LeftMotor_GetRPM,
};

static const MotorInterface_t right_motor = {
    .init      = RightMotor_Init,
    .set_speed = RightMotor_SetSpeed,
    .get_rpm   = RightMotor_GetRPM,
};

const MotorInterface_t *MotorFactory_GetMotor(MotorIndex_e index) {
    const MotorInterface_t *table[] = {&left_motor, &right_motor};
    return (index < 2) ? table[index] : NULL;
}
```

**✅ 最佳实践**

- 工厂返回的接口表用 `const` 修饰，数据存于 Flash
- 工厂函数应做参数校验，返回 `NULL` 表示无效类型
- 编译期能确定的类型多用宏/预处理工厂（零开销），运行期动态决定用函数工厂

---

### 3.3 观察者模式（Observer）

**定义**: 定义一对多的依赖关系，一个对象状态变化时，所有依赖者自动收到通知。

#### 适用场景

- **传感器数据更新**：IMU 数据就绪后，多个任务需要感知（PID 控制、姿态显示、数据记录）
- **按键事件分发**：按键按下时，不同的处理模块做出响应
- **通信数据到达**：串口接收到完整帧后，分发到不同解析器

#### 实现建议：回调注册机制

```c
// event_bus.h — 简单事件总线（观察者模式）
typedef void (*EventCallback_t)(void *context, uint32_t event_id, void *data);

typedef struct {
    EventCallback_t callback;
    void           *context;
} EventSubscription_t;

#define MAX_SUBSCRIBERS 8

typedef struct {
    EventSubscription_t subscribers[MAX_SUBSCRIBERS];
    uint8_t count;
} EventBus_t;

// 接口
void EventBus_Init(EventBus_t *bus);
int  EventBus_Subscribe(EventBus_t *bus, EventCallback_t cb, void *ctx);
void EventBus_Publish(EventBus_t *bus, uint32_t event_id, void *data);
void EventBus_UnsubscribeAll(EventBus_t *bus);
```

#### 本项目的现有观察者模式

[`CallBack.h`](Lib/01APP/Inc/CallBack.h) 中定义了回调函数，本质上可扩展为观察者模式：

```c
// 当前的回调机制（简化示例）
// 在 ISR 或传感器就绪时调用回调
void Sensor_DataReady(float accel_x, float accel_y, float accel_z);

// 可注册多个观察者
void PID_Controller_OnSensorUpdate(float x, float y, float z);
void Display_OnSensorUpdate(float x, float y, float z);
```

#### 中断安全的观察者

```c
// 为防止 ISR 中的竞争条件，使用"发布-延迟处理"模式：
volatile uint8_t g_sensor_event_flag = 0;
SensorData_t     g_sensor_event_data;

// ISR（或回调）中只设置标志
void Sensor_ISR(void) {
    g_sensor_event_data = read_sensor();
    g_sensor_event_flag = 1;          // 原子操作
}

// 主循环中处理
void ProcessEvents(void) {
    if (g_sensor_event_flag) {
        g_sensor_event_flag = 0;
        EventBus_Publish(&g_eventBus, EVENT_SENSOR_UPDATE, &g_sensor_event_data);
    }
}
```

**✅ 最佳实践**

- 使用**事件 ID 枚举**定义所有事件，避免 magic number
- 订阅者数量上限定义为宏（如 `MAX_SUBSCRIBERS 8`），防止动态分配
- ISR 中只设置 `volatile` 标志或发布极简消息，**不直接调用回调**
- 回调中不应有阻塞操作或 `HAL_Delay()`

---

## 4. SOLID 原则

### 4.1 单一职责原则（SRP）

> **一个模块/结构体应该只有一个引起它变化的原因。**

#### 在本项目中的应用

| ✅ 符合 SRP | ❌ 违反 SRP |
|------------|------------|
| [`Motor.c`](Lib/03BSP/Src/Motor.c) — 只负责电机 PWM 和方向控制 | 如果在 Motor.c 里同时处理编码器数据读取 |
| [`pid.c`](Lib/03BSP/Src/pid.c) — 只负责 PID 算法计算 | 如果在 pid.c 里混入串口输出逻辑 |
| [`Encoder.c`](Lib/03BSP/Src/Encoder.c) — 只负责编码器脉冲读取 | 在 Encoder 中同时做速度闭环控制 |

**检查方法**: 用一句话描述一个文件的功能，如果出现"和"、"以及"等连接词，说明可能承担了多个职责。

### 4.2 开闭原则（OCP）

> **对扩展开放，对修改关闭。** 增加新功能时，尽量不修改已有代码。

#### 在本项目中的应用

**最大亮点**: [`ModuleBase_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h) 虚函数表体系完美体现了 OCP。

添加一个新的设备模块（如 Buzzer）**不需要修改任何已有代码**：

```c
// 新增文件 — Buzzer.h / Buzzer.c
static const ModuleVTable_t buzzer_vtable = {
    .init    = Buzzer_init,
    .run     = Buzzer_run,
    .cleanup = Buzzer_cleanup,
};

// 使用 — 现有调度代码无需改动
ModuleBase_t *modules[] = {&led, &key, &buzzer};  // 直接添加
for (int i = 0; i < 3; i++) ModuleBase_Run(modules[i]);
```

**实践规则**:
- 新硬件模块优先考虑继承 `ModuleBase_t`，而不是修改框架
- 控制算法（如新 PID 变种）通过实现已有接口函数来扩展
- 通过编译宏（如 `#ifdef MOTOR_TYPE_MG310`）支持多配置，而不是修改既有代码逻辑

### 4.3 里氏替换原则（LSP）

> **子类对象必须能够替换基类对象，且程序行为不变。**

#### 在本项目中的应用

所有继承 [`ModuleBase_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h) 的子类必须满足：

```c
// 任何子类都必须能通过基类指针正常调用
ModuleBase_t *pModule = GetAnyModule();
int ret = ModuleBase_Init(pModule);    // 必须正常工作
ret = ModuleBase_Run(pModule);         // 必须正常工作
ModuleBase_Destructor(pModule);        // 必须正常工作
```

**违反 LSP 的典型情况**:

```c
// ❌ 危险：子类的 init 假设了 self 必须是子类类型
static int LED_init(void *self) {
    // 如果 self 实际上不是 LED_t*，强制转换会出问题
    LED_t *led = (LED_t *)self;
    // ...
}
```

**实践规则**:

- 子类实现的虚函数必须兼容基类的语义（init 做初始化，run 做运行逻辑）
- 虚函数不应对 `self` 指针做超出基类约定的假设
- 基类约定的前置条件（如 `initialized = 0` 时才能 init）必须遵守

### 4.4 接口隔离原则（ISP）

> **不应该强迫调用者依赖它不使用的方法。**

#### 在本项目中的应用

对比两种接口设计：

```c
// ❌ 臃肿接口：一个结构体包含所有外设操作
typedef struct {
    int (*init)(void);
    int (*send)(uint8_t *data, uint16_t len);
    int (*receive)(uint8_t *data, uint16_t len);
    int (*set_speed)(int16_t pwm);
    int (*read_angle)(float *angle);
    // ... 越来越多
} AllInOne_t;  // 每个外设都要实现不关心的函数

// ✅ 隔离接口：按职责拆分
typedef struct {  // 通信接口
    int (*send)(void *self, uint8_t *data, uint16_t len);
    int (*receive)(void *self, uint8_t *data, uint16_t len);
} CommInterface_t;

typedef struct {  // 运动控制接口
    int (*set_speed)(void *self, int16_t pwm);
    int (*stop)(void *self);
} MotionInterface_t;
```

**实践规则**:

- 每个头文件只暴露一个明确职责的接口集合
- [`BSP_Headers.h`](Lib/03BSP/Inc/BSP_Headers.h) 是聚合头文件，不定义新接口
- 接口函数参数尽量少（不超过 4 个），多参数应考虑封装为结构体

### 4.5 依赖倒置原则（DIP）

> **高层模块不应依赖低层模块，两者都应依赖抽象。抽象不应依赖细节，细节应依赖抽象。**

#### 在本项目中的应用

```
✅ 正确依赖关系：

  应用层 (Task.c)
      ↓ 依赖抽象接口
  控制层 (Move_Control.c) ——→ 定义抽象："设置速度、停止"
      ↓ 依赖抽象接口
  BSP层 (Motor.c)          ——→ 实现抽象接口
      ↓ 调用
  HAL 库函数                ——→ 硬件寄存器操作

❌ 错误依赖关系：

  应用层 ——→ 直接调用 HAL_GPIO_WritePin() 控制电机
```

**实践规则**:

- 上层代码（APP/COM）**只能**通过头文件定义的接口函数调用下层，不能直接操作 HAL 句柄或寄存器
- 接口定义应放在**调用方所在的层级**（如控制层定义 "运动接口"，驱动层去实现）
- 使用编译期依赖注入：通过结构体函数指针表允许替换实现（便于测试和硬件变更）

---

## 5. 推荐的组织方式

### 5.1 模块/命名空间划分

本项目使用 **目录即命名空间** 的惯例：

| 目录 | 命名空间 | 职责 |
|------|---------|------|
| [`Lib/01APP/`](Lib/01APP/) | `APP_` | 应用层：任务调度、业务逻辑 |
| [`Lib/02COM/`](Lib/02COM/) | `COM_` / 无前缀 | 控制层：运动控制、状态机、通信 |
| [`Lib/03BSP/`](Lib/03BSP/) | 模块前缀如 `Motor_`, `PID_`, `Encoder_` | 板级支持包：硬件驱动 |
| [`Devices/Inc/DeviceClass/`](Devices/Inc/DeviceClass/) | `ModuleBase_` | 设备抽象基类体系 |
| [`Core/`](Core/) | 框架前缀 `scheduler_`, `state_machine_` | 框架层：调度器、状态机 |

**命名规则**:

- 所有公有函数以**模块名_动作** 格式命名：`PID_Calculate()`, `Motor_SetPWM()`
- 类型定义以 `_t` 结尾：`PID_Controller_t`, `Task_t`, `encoder_t`
- 枚举以 `_e` 结尾：`MotorIndex_e`, `PID_Type_e`, `Task_e`
- 内部 `static` 函数无需模块前缀，但应清晰描述功能

### 5.2 类（结构体）设计规范

```c
// ✅ 推荐的"类"设计模板
// ===== xxx.h =====

// 1. 前置声明
typedef struct XXX_s XXX_t;

// 2. 虚函数表（如果需要多态）
typedef struct {
    int (*do_something)(void *self, int arg);
} XXX_VTable_t;

// 3. 结构体定义
struct XXX_s {
    const XXX_VTable_t *vtable;   // 可选：虚函数表
    // 公有成员（只读）
    uint8_t status;
    // 私有成员（不直接访问）
    int internal_data;
};

// 4. 公有接口
void XXX_Init(XXX_t *self);
int  XXX_DoAction(XXX_t *self, int arg);

// ===== xxx.c =====
// 5. 私有函数用 static 隐藏
static int XXX_internalHelper(XXX_t *self) { ... }
```

### 5.3 接口设计惯例

| 模式 | 说明 | 示例 |
|------|------|------|
| **Init/Deinit 配对** | 每个模块提供初始化和反初始化 | `PID_Init()` |
| **Get/Set 命名** | 读值用 Get，写值用 Set | `PID_GetOutput()`, `PID_SetTarget()` |
| **Handle 模式** | 通过指针操作实例 | `PID_Calculate(target, feedback, &pid)` |
| **返回 int 状态** | 0=成功，负数=错误 | 所有 HAL 调用都应检查返回值 |
| **Config 结构体** | 多个参数用结构体传入 | `PID_SetParams(&pid, {.kp=1.0, .ki=0.5})` |

### 5.4 文件组织规则

```
每个模块应遵循"一个头文件 + 一个源文件"原则：

Motor.h          ← 公有接口（结构体定义、函数声明、常量宏）
Motor.c          ← 实现细节（static 函数、static 变量）

聚合头文件用于上层便捷引用：
BSP_Headers.h    ← #include 所有 BSP 模块的头文件
COM_Headers.h    ← #include 所有 COM 模块的头文件
APP_Headers.h    ← #include 所有 APP 模块的头文件
```

**头文件包含顺序**（保持清晰依赖关系）：

```c
// xxx.c 的包含顺序
#include "xxx.h"            // 1. 自身的头文件（最先包含，检查独立性）
#include "Config.h"          // 2. 项目配置
#include "Variable.h"        // 3. 全局变量声明
#include "ModuleBase.h"      // 4. 框架/基类
#include "Motor.h"           // 5. 同层模块
#include "stm32f4xx_hal.h"   // 6. 底层 HAL（最后）
```

---

## 附录：快速参考

### 常用 OOP 惯用代码片段

#### 1. 定义一个"类"（结构体 + 方法）

```c
// Counter.h
typedef struct {
    int value;
    int max;
} Counter_t;

void Counter_Init(Counter_t *self, int max);
void Counter_Increment(Counter_t *self);
int  Counter_GetValue(const Counter_t *self);
int  Counter_IsMax(const Counter_t *self);

// Counter.c
void Counter_Init(Counter_t *self, int max) {
    self->value = 0;
    self->max = max;
}
void Counter_Increment(Counter_t *self) {
    if (self->value < self->max) self->value++;
}
```

#### 2. 定义一个接口（纯虚函数表）

```c
// SensorInterface.h
typedef struct {
    int (*init)(void *self);
    int (*read)(void *self, float *data);
    int (*reset)(void *self);
} SensorInterface_t;

// 通过接口调用
int Sensor_Read(void *self, const SensorInterface_t *ops, float *data) {
    if (self == NULL || ops == NULL || ops->read == NULL) return -1;
    return ops->read(self, data);
}
```

#### 3. 多态数组遍历

```c
// 统一管理所有模块
ModuleBase_t *all_modules[] = {
    (ModuleBase_t *)&module_led,
    (ModuleBase_t *)&module_key,
    (ModuleBase_t *)&module_oled,
};

void System_InitAll(void) {
    for (int i = 0; i < MODULE_COUNT; i++) {
        if (ModuleBase_Init(all_modules[i]) != 0) {
            // 错误处理
        }
    }
}
```

### 检查清单

编写或审查代码时，对照以下问题：

- [ ] 是否每个结构体只负责一个明确的职责？（SRP）
- [ ] 增加新功能时是否不需要修改已有代码？（OCP）
- [ ] 子类是否能安全替换基类指针？（LSP）
- [ ] 接口是否最小化，没有强迫调用者依赖不用的方法？（ISP）
- [ ] 高层代码是否依赖抽象接口而非具体实现？（DIP）
- [ ] 内部实现是否用 `static` 隐藏了？（封装）
- [ ] 全局/静态共享变量是否用 `volatile` 修饰了？（中断安全）
- [ ] 所有 HAL 调用是否检查了返回值？（健壮性）

---

> **文档维护**: 本文档应与实际代码保持同步。当项目引入新的 OOP 模式或结构体设计规范时，请及时更新。
