/**
 * @file state_machine.h
 * @brief 通用有限状态机（FSM）框架
 *
 * 这个模块提供基础的状态机框架，支持状态转移和回调。
 * 设计模式：状态进入(enter)、处理(process)、退出(exit)三阶段模式
 *
 * 使用流程：
 * 1. 定义状态枚举
 * 2. 为每个状态定义 enter/process/exit 回调
 * 3. 创建状态处理器表
 * 4. 初始化状态机
 * 5. 在任务中调用 state_machine_update()
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

/**
 * @brief 状态回调函数类型
 */
typedef void (*state_callback_t)(void);

/**
 * @brief 状态处理器结构体
 * 每个状态有三个回调：进入、处理、退出
 */
typedef struct {
    state_callback_t enter;     /**< 进入该状态时调用 */
    state_callback_t process;   /**< 每次更新时调用 */
    state_callback_t exit;      /**< 离开该状态时调用 */
} StateHandler_t;

/**
 * @brief 状态机结构体
 * @note 用户应为不同的应用创建自己的扩展结构体
 */
typedef struct {
    uint8_t current_state;      /**< 当前状态 */
    uint8_t next_state;         /**< 下一个状态 */
    const StateHandler_t *handlers;  /**< 状态处理器表 */
    uint8_t num_states;         /**< 状态总数 */
    uint8_t first_update;       /**< 首次更新标志 */
} StateMachine_t;

/**
 * @brief 初始化状态机
 * @param fsm: 状态机指针
 * @param handlers: 状态处理器表
 * @param num_states: 状态数
 * @param initial_state: 初始状态
 */
void state_machine_init(StateMachine_t *fsm,
                        const StateHandler_t *handlers,
                        uint8_t num_states,
                        uint8_t initial_state);

/**
 * @brief 状态机更新
 * 应在任务调度器中定期调用（通常每 10-50ms 一次）
 *
 * 流程：
 * 1. 检查状态是否变化
 * 2. 如果变化：调用旧状态的 exit，新状态的 enter
 * 3. 调用当前状态的 process
 */
void state_machine_update(StateMachine_t *fsm);

/**
 * @brief 设置下一个状态
 * 状态变化会在下次 update 时生效
 */
void state_machine_set_state(StateMachine_t *fsm, uint8_t next_state);

/**
 * @brief 获取当前状态
 */
uint8_t state_machine_get_state(const StateMachine_t *fsm);

/**
 * @brief 获取下一个状态
 */
uint8_t state_machine_get_next_state(const StateMachine_t *fsm);

#endif // STATE_MACHINE_H
