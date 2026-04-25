/**
 * @file state_machine.c
 * @brief 通用有限状态机实现
 */

#include "state_machine.h"
#include <stddef.h>

void state_machine_init(StateMachine_t *fsm,
                        const StateHandler_t *handlers,
                        uint8_t num_states,
                        uint8_t initial_state) {
    if (fsm == NULL || handlers == NULL) return;

    fsm->handlers = handlers;
    fsm->num_states = num_states;
    fsm->current_state = initial_state;
    fsm->next_state = initial_state;
    fsm->first_update = 1;
}

void state_machine_update(StateMachine_t *fsm) {
    if (fsm == NULL || fsm->handlers == NULL) return;

    if (fsm->current_state >= fsm->num_states) return;

    if (fsm->first_update) {
        fsm->first_update = 0;

        if (fsm->handlers[fsm->current_state].enter) {
            fsm->handlers[fsm->current_state].enter();
        }
    }

    if (fsm->current_state != fsm->next_state) {
        if (fsm->handlers[fsm->current_state].exit) {
            fsm->handlers[fsm->current_state].exit();
        }

        fsm->current_state = fsm->next_state;

        if (fsm->handlers[fsm->current_state].enter) {
            fsm->handlers[fsm->current_state].enter();
        }
    }

    if (fsm->handlers[fsm->current_state].process) {
        fsm->handlers[fsm->current_state].process();
    }
}

void state_machine_set_state(StateMachine_t *fsm, uint8_t next_state) {
    if (fsm != NULL && next_state < fsm->num_states) {
        fsm->next_state = next_state;
    }
}

uint8_t state_machine_get_state(const StateMachine_t *fsm) {
    return fsm != NULL ? fsm->current_state : 0;
}

uint8_t state_machine_get_next_state(const StateMachine_t *fsm) {
    return fsm != NULL ? fsm->next_state : 0;
}
