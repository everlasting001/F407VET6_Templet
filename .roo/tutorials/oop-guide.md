# 🧱 面向对象编程指南（OOP Guide）

> **适用项目**: STM32F407VET6 嵌入式工程  
> **语言**: C（通过结构体 + 函数指针模拟 OOP）  
> **目标读者**: 有基础编程经验、需了解本项目 OOP 惯例的开发者

---

## 📑 目录

- [1. 概述](#1-概述)
- [2. 四大基本特性](#2-四大基本特性)
  - [2.1 封装（Encapsulation）](#21-封装encapsulation)
  - [2.2 继承（Inheritance）](#22-继承inheritance)
  - [2.3 多态（Polymorphism）](#23-多态polymorphism)
  - [2.4 抽象（Abstraction）](#24-抽象abstraction)
- [3. 设计模式](#3-设计模式)
  - [3.1 单例模式（Singleton）](#31-单例模式singleton)
  - [3.2 工厂模式（Factory）](#32-工厂模式factory)
  - [3.3 观察者模式（Observer）](#33-观察者模式observer)
- [4. SOLID 原则](#4-solid-原则)
  - [4.1 单一职责原则（SRP）](#41-单一职责原则srp)
  - [4.2 开闭原则（OCP）](#42-开闭原则ocp)
  - [4.3 里氏替换原则（LSP）](#43-里氏替换原则lsp)
  - [4.4 接口隔离原则（ISP）](#44-接口隔离原则isp)
  - [4.5 依赖反转原则（DIP）](#45-依赖反转原则dip)
- [5. 模块与文件组织规范](#5-模块与文件组织规范)
  - [5.1 目录结构约定](#51-目录结构约定)
  - [5.2 命名规范](#52-命名规范)
  - [5.3 头文件设计准则](#53-头文件设计准则)
- [6. 快速参考](#6-快速参考)

---

## 1. 概述

本工程使用 **C 语言**开发，但在架构层面大量运用了面向对象的设计思想。由于 C 语言没有内建的 `class`、`interface`、`inheritance` 语法，我们通过以下机制模拟 OOP：

| OOP 特性 | C 语言实现方式 | 本工程示例 |
|----------|---------------|-----------|
| 封装 | `结构体` + `头文件隐藏实现` | [`ModuleBase_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h:78) |
| 继承 | `结构体嵌套`（基类作为第一个成员） | [`ModuleBase_t` 作为子类首个成员](Devices/Inc/DeviceClass/Modules/ModuleBase.h:13) |
| 多态 | `函数指针虚表（VTable）` | [`ModuleVTable_t`](Devices/Inc/DeviceClass/Modules/ModuleBase.h:63) |
| 抽象 | `仅声明接口` + `默认空实现` | [`default_vtable`](Devices/Src/DeviceClass/Modules/ModuleBase.c:89) |

---

## 2. 四大基本特性

### 2.1 封装（Encapsulation）

**定义**：将数据和操作封装在结构体内，对外暴露接口，隐藏实现细节。

**本工程实践**：

```c
// ===== ModuleBase.h（公开接口） =====
typedef struct ModuleBase_s {
    const ModuleVTable_t *vtable;   // 虚函数表（内部机制，使用者不应直接操作）
    const char           *name;     // 模块名称
    uint8_t               initialized; // 初始化标志
} ModuleBase_t;

// 对外只暴露操作函数，不暴露内部实现
void ModuleBase_Constructor(ModuleBase_t *self, const char *name);
void ModuleBase_Destructor(ModuleBase_t *self);
int  ModuleBase_Init(ModuleBase_t *self);
int  ModuleBase_Run(ModuleBase_t *self);
void ModuleBase_Cleanup(ModuleBase_t *self);
const char *ModuleBase_GetName(const ModuleBase_t *self);
uint8_t ModuleBase_IsInitialized(const ModuleBase_t *self);
```

```c
// ===== ModuleBase.c（隐藏实现） =====
// 默认虚函数使用 static 关键字，外部不可见
static int ModuleBase_defaultInit(void *self) { (void)self; return 0; }
static int ModuleBase_defaultRun(void *self) { (void)self; return 0; }

// 默认虚函数表也是 static 的
static const ModuleVTable_t default_vtable = {
    .init    = ModuleBase_defaultInit,
    .run     = ModuleBase_defaultRun,
    .cleanup = ModuleBase_defaultCleanup,
    .reset   = ModuleBase_defaultReset,
};
```

**规则**：
- `.h` 文件只暴露公有接口和类型定义
- `.c` 文件中的辅助函数和默认实现全部用 `static` 修饰
- 结构体成员若为内部状态，通过公有 getter/setter 访问（如 [`ModuleBase_GetName()`](Devices/Src/DeviceClass/Modules/ModuleBase.c:236)、[`ModuleBase_IsInitialized()`](Devices/Src/DeviceClass/Modules/ModuleBase.c:249)）

---

### 2.2 继承（Inheritance）

**定义**：子类复用基类的数据和接口，并扩展自有成员。

**本工程实践**——通过结构体嵌套模拟单继承：

```c
// ===== 基类定义（ModuleBase.h） =====
typedef struct ModuleBase_s {
    const ModuleVTable_t *vtable;
    const char           *name;
    uint8_t               initialized;
} ModuleBase_t;

// ===== 子类定义（例如 LED 模块） =====
typedef struct {
    ModuleBase_t base;          // ⚠️ 基类必须为第一个成员（确保指针可安全转换）
    uint16_t     gpio_pin;      // 子类自有成员
    GPIO_TypeDef *gpio_port;    // 子类自有成员
} LED_t;

// ===== 子类构造 =====
LED_t led;
ModuleBase_Constructor(&led.base, "LED");  // 调用基类构造
led.base.vtable = &led_vtable;             // 替换为子类虚函数表
led.gpio_pin    = LED1_Pin;                // 初始化自有成员
led.gpio_port   = LED1_GPIO_Port;
```

**关键规则**：
1. 基类结构体**必须**作为子类结构体的**第一个成员**——这保证了 `(ModuleBase_t *)&led` 的指针转换是安全的
2. 构造时先调用 [`ModuleBase_Constructor()`](Devices/Src/DeviceClass/Modules/ModuleBase.c:105) 初始化基类部分，再设置子类特有数据
3. 子类可以选择重写虚函数（通过替换 vtable），也可以使用基类的默认实现

---

### 2.3 多态（Polymorphism）

**定义**：同一接口在不同子类中有不同的行为表现。

**本工程实践**——通过虚函数表（VTable）实现：

```c
// ===== 虚函数表定义（ModuleBase.h:63） =====
typedef struct {
    int  (*init)(void *self);       // 初始化
    int  (*run)(void *self);        // 运行
    int  (*cleanup)(void *self);    // 清理
    void (*reset)(void *self);      // 复位
} ModuleVTable_t;

// ===== 子类定义自己的虚函数表 =====
static int LED_init(void *self) {
    LED_t *led = (LED_t *)self;
    HAL_GPIO_WritePin(led->gpio_port, led->gpio_pin, GPIO_PIN_SET);
    return 0;
}

static int LED_cleanup(void *self) {
    LED_t *led = (LED_t *)self;
    HAL_GPIO_WritePin(led->gpio_port, led->gpio_pin, GPIO_PIN_RESET);
    return 0;
}

static const ModuleVTable_t led_vtable = {
    .init    = LED_init,
    .run     = NULL,          // 使用基类默认 run（空实现）
    .cleanup = LED_cleanup,
    .reset   = NULL,
};

// ===== 多态调用 =====
ModuleBase_t *devices[] = {
    (ModuleBase_t *)&led1,
    (ModuleBase_t *)&buzzer,
    (ModuleBase_t *)&key,
};

for (int i = 0; i < 3; i++) {
    ModuleBase_Init(devices[i]);     // ⚡ 实际调用各自子类的 init
    ModuleBase_Run(devices[i]);      // ⚡ 各自行为不同
}
```

**多态的实现路径**：

```
ModuleBase_Init(p)
    └→ p->vtable->init(p)      // 通过 vtable 跳转到子类实现
         └→ LED_init(p)        // 或 Buzzer_init(), Key_init() ...
```

**优势**：上层代码（如 `main.c` 或任务调度器）无需知道具体子类类型，只需操作 `ModuleBase_t *` 指针即可统一管理所有设备。

---

### 2.4 抽象（Abstraction）

**定义**：只定义接口契约，隐藏具体实现，使用者仅关心"能做什么"而非"怎么做"。

**本工程实践**：

```c
// ===== 虚函数表就是接口契约 =====
// 任何模块只要实现了 ModuleVTable_t 中的函数指针，
// 就可以被视为一个"模块"，可以在框架中统一管理。

// ===== 基类提供默认空实现，子类按需重写 =====
static int ModuleBase_defaultInit(void *self) {
    (void)self;
    return 0;  // 默认"什么都不做"也是一种实现
}

// ===== 公有接口屏蔽内部复杂性 =====
int ModuleBase_Init(ModuleBase_t *self) {
    if ((self == NULL) || (self->vtable == NULL)) return -1;
    if (self->initialized) return 0;  // 防止重复初始化
    if (self->vtable->init != NULL) {
        return self->vtable->init((void *)self);
    }
    return 0;
}
```

**使用者视角**：

```c
// 使用者不需要知道 LED_init 内部的 GPIO 操作细节
// 只需要调用统一的 ModuleBase_Init()
ModuleBase_Init((ModuleBase_t *)&my_led);
ModuleBase_Init((ModuleBase_t *)&my_buzzer);
```

> 抽象的核心是**接口与实现分离**。在本工程中，`ModuleVTable_t` 就是最高层次的抽象契约。

---

## 3. 设计模式

### 3.1 单例模式（Singleton）

**适用场景**：硬件外设（如 UART、ADC、定时器）在 MCU 中只有一个实例，需要全局唯一访问点。

**实现建议**：

```c
// ===== debug_uart.h =====
typedef struct {
    UART_HandleTypeDef *handle;
    uint8_t             initialized;
} DebugUART_t;

// 全局唯一的访问函数
DebugUART_t *DebugUART_GetInstance(void);

int  DebugUART_Init(void);
int  DebugUART_Send(const uint8_t *data, uint16_t len);
void DebugUART_SetHandle(UART_HandleTypeDef *huart);

// ===== debug_uart.c =====
static DebugUART_t instance = {0};  // 全局唯一实例

DebugUART_t *DebugUART_GetInstance(void) {
    return &instance;
}

void DebugUART_SetHandle(UART_HandleTypeDef *huart) {
    instance.handle = huart;
}

int DebugUART_Init(void) {
    if (instance.handle == NULL) return -1;
    instance.initialized = 1;
    return 0;
}

int DebugUART_Send(const uint8_t *data, uint16_t len) {
    if (!instance.initialized || instance.handle == NULL) return -1;
    return (HAL_UART_Transmit(instance.handle, data, len, HAL_MAX_DELAY) == HAL_OK) ? 0 : -1;
}
```

**适用本工程的场景**：
- **调试串口**（`huart1`）—— 全局只需一个调试输出通道
- **MPU6050 I2C 句柄**—— I2C 总线在系统中只有一个实例
- **任务调度器**（[`scheduler_add_task()`](ARCHITECTURE_SUMMARY.md:239)）—— 全局只需一个调度器实例

---

### 3.2 工厂模式（Factory）

**适用场景**：需要根据配置或运行时条件动态创建不同类型的模块对象。

**实现建议**：

```c
// ===== device_factory.h =====
typedef enum {
    DEVICE_TYPE_LED,
    DEVICE_TYPE_BUZZER,
    DEVICE_TYPE_KEY,
    DEVICE_TYPE_OLED,
} DeviceType_t;

/**
 * @brief 设备工厂：创建并初始化一个设备模块
 * @param type    设备类型
 * @param name    模块名称
 * @param out_dev 输出参数，指向创建好的模块基类指针
 * @return int    0 = 成功, -1 = 不支持的类型
 */
int DeviceFactory_Create(DeviceType_t type, const char *name, ModuleBase_t **out_dev);

// ===== device_factory.c =====
// 注意：实际使用中需要为每种设备分配静态存储或使用内存池
// 本工程禁用 malloc，因此"创建"通常指初始化一个预定义的静态对象

static LED_t    led_pool[MAX_LED_COUNT];
static Buzzer_t buzzer_pool[MAX_BUZZER_COUNT];
static Key_t    key_pool[MAX_KEY_COUNT];

int DeviceFactory_Create(DeviceType_t type, const char *name, ModuleBase_t **out_dev) {
    switch (type) {
        case DEVICE_TYPE_LED: {
            static int led_index = 0;
            if (led_index >= MAX_LED_COUNT) return -1;
            LED_t *led = &led_pool[led_index++];
            ModuleBase_Constructor(&led->base, name);
            led->base.vtable = &led_vtable;         // 绑定 LED 的虚函数表
            *out_dev = (ModuleBase_t *)led;
            return 0;
        }
        case DEVICE_TYPE_BUZZER: {
            // ... 类似逻辑
        }
        default:
            return -1;  // 不支持的类型
    }
}
```

**适用本工程的场景**：
- **模块初始化入口**—— 在 `app_init()` 中根据配置批量创建设备
- **传感器探测**—— 运行时检测到 I2C 设备地址后，创建对应的驱动实例
- **多电机管理**—— 根据电机类型（直流/步进）创建不同的控制接口

---

### 3.3 观察者模式（Observer）

**适用场景**：一个事件源（如按键按下、传感器阈值触发）需要通知多个接收者。

**实现建议**——使用中断标志 + 轮询的轻量变体（嵌入式环境避免回调注册的复杂性）：

```c
// ===== event_bus.h =====
#define MAX_SUBSCRIBERS  8

typedef void (*EventHandler_t)(void *context);

typedef struct {
    EventHandler_t handler;
    void          *context;
} Subscriber_t;

typedef struct {
    Subscriber_t subscribers[MAX_SUBSCRIBERS];
    uint8_t      count;
} EventBus_t;

void EventBus_Init(EventBus_t *bus);
int  EventBus_Subscribe(EventBus_t *bus, EventHandler_t handler, void *context);
void EventBus_Publish(EventBus_t *bus);
```

```c
// ===== 使用示例：按键事件驱动多个模块 =====
static void on_key_pressed(void *context) {
    // 切换 LED 状态
    ModuleBase_t *led = (ModuleBase_t *)context;
    // ... toggle LED
}

static void on_key_released(void *context) {
    // 触发蜂鸣器
    ModuleBase_t *buzzer = (ModuleBase_t *)context;
    // ... beep
}

// 在初始化中注册观察者
EventBus_t key_event_bus;
EventBus_Init(&key_event_bus);
EventBus_Subscribe(&key_event_bus, on_key_pressed, &led);
EventBus_Subscribe(&key_event_bus, on_key_released, &buzzer);

// 在主循环中检测按键并发布事件
if (key_pressed_flag) {
    key_pressed_flag = 0;
    EventBus_Publish(&key_event_bus);  // 通知所有订阅者
}
```

**更轻量的替代方案**——使用回调函数指针（参考 HAL 库模式）：

```c
// 按键模块结构体中包含回调注册
typedef struct {
    ModuleBase_t base;
    void (*on_press)(void);     // 按键按下回调
    void (*on_release)(void);   // 按键释放回调
} Key_t;

// 注册回调
void Key_RegisterCallbacks(Key_t *key, void (*on_press)(void), void (*on_release)(void)) {
    key->on_press   = on_press;
    key->on_release = on_release;
}
```

**适用本工程的场景**：
- **按键中断** → 切换 LED、触发蜂鸣器、切换模式
- **串口数据到达** → 解析命令、更新状态、触发响应
- **灰度传感器检测到黑线** → 触发 PID 调整、记录位置
- **MPU6050 数据就绪** → 更新姿态、触发控制算法

---

## 4. SOLID 原则

SOLID 是面向对象设计的五个基本原则。虽然在 C 语言中实现不如 C++/Java 直接，但其思想对本工程的架构设计同样有重要指导意义。

### 4.1 单一职责原则（SRP）

> **一个模块/结构体只应有一个引起它变化的原因。**

**本工程实践**：

| ✅ 好的做法 | ❌ 避免的做法 |
|------------|-------------|
| `ModuleBase_t` 只负责模块生命周期管理 | 在电机驱动中混合 PID 控制逻辑 |
| `Motor_t` 只负责电机硬件控制 | 在传感器驱动中直接写业务逻辑 |
| 任务调度器只负责调度，不负责业务 | 在 ISR 中处理复杂的业务逻辑 |

```c
// ✅ 职责分离：各模块各司其职
// Core/Drivers/   → 设备驱动（硬件操作）
// Core/Framework/ → 框架层（调度、状态机）
// Core/App/       → 应用层（业务逻辑）
```

### 4.2 开闭原则（OCP）

> **对扩展开放，对修改关闭。**

**本工程实践**——通过虚函数表实现：

```c
// ✅ 新增一个 OLED 模块，不需要修改框架层的任何代码
static const ModuleVTable_t oled_vtable = {
    .init    = OLED_init,
    .run     = OLED_run,
    .cleanup = OLED_cleanup,
    .reset   = OLED_reset,
};

// 直接加入设备数组即可
ModuleBase_t *devices[] = {
    (ModuleBase_t *)&led,
    (ModuleBase_t *)&buzzer,
    (ModuleBase_t *)&oled,    // ✅ 新增，无须修改框架
};
```

**验证方式**：新增模块时，检查是否修改了以下文件：
- 不应修改：`ModuleBase.h`、`ModuleBase.c`、`task_scheduler.c`、`state_machine.c`
- 应新建：`OLED.h`、`OLED.c`（或放入 `Devices/` 目录）

### 4.3 里氏替换原则（LSP）

> **子类应能替换父类而不影响程序正确性。**

**本工程实践**——严格要求基类指针安全转换：

```c
// ✅ 任何子类对象都可以安全地当作 ModuleBase_t* 使用
void ProcessDevice(ModuleBase_t *dev) {
    ModuleBase_Init(dev);     // 无论传入 LED/Buzzer/OLED 都正常工作
    ModuleBase_Run(dev);
}

// 子类结构体的第一个成员必须是 ModuleBase_t
typedef struct {
    ModuleBase_t base;    // ⚠️ 必须是第一个，不能颠倒顺序
    // ... 子类自有成员
} SomeDevice_t;
```

**违反 LSP 的常见错误**：
- 基类不是子类结构体的**第一个成员** → ❌ `void *` 转换将指向错误地址
- 子类 `init()` 函数中假设了错误的类型转换 → ❌ 应始终 `(Base_t *)self` 转基类指针
- 子类 `run()` 函数执行了阻塞操作 → ❌ 违背了基类"快速返回"的约定

### 4.4 接口隔离原则（ISP）

> **不应强迫模块依赖它们不需要的接口。**

**本工程实践**：

```c
// ✅ ModuleVTable_t 只包含最基本的 4 个方法
typedef struct {
    int  (*init)(void *self);       // 必须：初始化
    int  (*run)(void *self);        // 必须：运行
    int  (*cleanup)(void *self);    // 可选：清理
    void (*reset)(void *self);      // 可选：复位
} ModuleVTable_t;
```

如果某个模块需要额外功能（如电机需要 `set_speed`），不应往基类 vtable 中添加，而应：

```c
// ✅ 在子类中扩展独立的操作接口
typedef struct {
    ModuleBase_t base;
    // ... 电机特有成员
} Motor_t;

// 电机特有的接口（不在 ModuleVTable_t 中）
int Motor_SetSpeed(Motor_t *motor, uint16_t speed);
int Motor_SetDirection(Motor_t *motor, uint8_t dir);

// 调用时先安全检查，再调用子类特有方法
Motor_t *motor = (Motor_t *)device;
Motor_SetSpeed(motor, 500);
```

### 4.5 依赖反转原则（DIP）

> **高层模块不应依赖低层模块，两者都应依赖抽象。**

**本工程实践**——分层架构中的依赖方向：

```
  应用层（Core/App/）
       ↓ 依赖
  设备驱动抽象（ModuleBase_t / ModuleVTable_t）
       ↓ 依赖
  具体设备实现（LED.c, Motor.c, OLED.c ...）
       ↓ 依赖
  HAL 库（stm32f4xx_hal.h）
```

```c
// ✅ 高层代码（main.c / app_config.c）只依赖 ModuleBase_t 抽象，不依赖具体设备
void app_init(void) {
    for (int i = 0; i < device_count; i++) {
        ModuleBase_Init(devices[i]);   // 只依赖抽象
    }
}

// ❌ 避免：高层代码直接操作硬件寄存器
void app_init(void) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);  // 直接依赖低层
}
```

**DIP 的关键收益**：更换硬件平台时，只需替换具体设备驱动实现，上层应用代码无需修改。

---

## 5. 模块与文件组织规范

### 5.1 目录结构约定

```
Devices/                          ← 设备模块层（硬件驱动 + 业务模块）
├── Inc/
│   ├── DeviceClass/              ← 设备类定义
│   │   ├── Modules/              ← 通用模块（LED、KEY、OLED、BUZZER 等）
│   │   │   └── ModuleBase.h      ← 模块基类（虚函数表 + 构造/析构）
│   │   ├── Motors/               ← 电机类驱动
│   │   └── Sensors/              ← 传感器类驱动
│   ├── DebugPeripheral/          ← 调试外设
│   └── FilterAlgorithm/          ← 滤波算法
├── Src/
│   └── DeviceClass/Modules/
│       └── ModuleBase.c          ← 基类实现

Core/                             ← 核心框架和应用
├── Inc/
│   ├── main.h
│   ├── gpio.h, tim.h, usart.h    ← CubeMX 生成的外设头文件
├── Src/
│   ├── main.c                    ← 程序入口
│   ├── gpio.c, tim.c, usart.c    ← CubeMX 生成的外设初始化

Drivers/                          ← 厂商 HAL 库（不修改）
└── STM32F4xx_HAL_Driver/
```

### 5.2 命名规范

| 元素 | 约定 | 示例 |
|------|------|------|
| **类型/结构体** | `PascalCase_t` | `ModuleBase_t`, `ModuleVTable_t`, `LED_t` |
| **公有函数** | `ModuleName_Action()` | `ModuleBase_Init()`, `Motor_SetSpeed()` |
| **静态函数** | `ModuleName_defaultAction()` | `ModuleBase_defaultInit()` |
| **虚函数表变量** | `模块名小写_vtable` | `led_vtable`, `buzzer_vtable` |
| **宏/常量** | `大写_下划线` | `MAX_SUBSCRIBERS`, `DEVICE_TYPE_LED` |
| **文件命名** | `PascalCase` | `ModuleBase.h`, `MotorDriver.c` |

### 5.3 头文件设计准则

每个模块的头文件应遵循以下结构：

```c
/**
  * @file    ModuleName.h
  * @brief   模块简要描述
  */

#ifndef __MODULE_NAME_H__
#define __MODULE_NAME_H__

#include <stdint.h>

/* ==================== 类型定义 ==================== */

/* ==================== 公有接口函数 ==================== */

/* ==================== 内联函数（可选） ==================== */

#endif /* __MODULE_NAME_H__ */
```

**Include Guard 命名规则**：`__模块名_H__`（如 `__MODULE_BASE_H__`）

---

## 6. 快速参考

### 创建设备模块的步骤

```
1. 定义子类结构体（基类作为第一个成员）
       ↓
2. 实现子类的虚函数（init / run / cleanup / reset）
       ↓
3. 定义子类的虚函数表（static const）
       ↓
4. 声明子类对象 + 构造（ModuleBase_Constructor）
       ↓
5. 替换 vtable 指针（指向子类虚函数表）
       ↓
6. 通过 ModuleBase_Init/Run/Cleanup 多态调用
```

### 关键文件索引

| 文件 | 说明 |
|------|------|
| [`Devices/Inc/DeviceClass/Modules/ModuleBase.h`](Devices/Inc/DeviceClass/Modules/ModuleBase.h) | 基类定义、虚函数表、公有接口声明 |
| [`Devices/Src/DeviceClass/Modules/ModuleBase.c`](Devices/Src/DeviceClass/Modules/ModuleBase.c) | 默认实现、构造/析构、NULL 安全处理 |
| [`Core/Src/main.c`](Core/Src/main.c) | 程序入口，展示初始化顺序 |
| [`ARCHITECTURE_SUMMARY.md`](ARCHITECTURE_SUMMARY.md) | 项目分层架构总览 |
| [`CLAUDE.md`](CLAUDE.md) | 项目整体规范和构建指南 |

### 常见反模式（Anti-Patterns）

| ❌ 反模式 | ✅ 正确做法 |
|----------|------------|
| 直接访问 `ModuleBase_t` 的内部成员 | 使用公有接口函数 |
| 在一个 `.c` 文件中塞入多个不相关的模块 | 每个模块独立文件 |
| 往基类 vtable 中添加与生命周期无关的方法 | 在子类中扩展独立接口 |
| 在 ISR 中调用模块的业务逻辑 | ISR 中仅设标志，主循环中处理 |
| 使用 `malloc` 动态创建设备对象 | 使用静态全局对象或内存池 |

---

> 📖 **延伸阅读**：  
> - 项目分层架构详情见 [`ARCHITECTURE_SUMMARY.md`](ARCHITECTURE_SUMMARY.md)  
> - 状态机框架见 [`state_machine.h`](ARCHITECTURE_SUMMARY.md:249)（位于 Core/Framework/Inc/）  
> - 任务调度器见 [`task_scheduler.h`](ARCHITECTURE_SUMMARY.md:237)（位于 Core/Framework/Inc/）  
> - 电机驱动示例见 [`motor_driver.h`](ARCHITECTURE_SUMMARY.md:168)（位于 Core/Drivers/Inc/）
