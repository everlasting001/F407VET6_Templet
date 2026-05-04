#include "State_Machine.h"
#include "Config.h"
#include "Move_Control.h"

void Task1_Init(Task_t *task){
    task->task = Task_In;
}

void Task2_Init(Task_t *task){
    task->task = Task_In;
}

void Task3_Init(Task_t *task){
    task->task = Task_In;
}

void Task4_Init(Task_t *task){
    task->task = Task_In;
}

void Task1_StateMachine(Task_t *task){
    static uint8_t straight_count = 0;
    static uint8_t need_reset = 1;  // 表示当前状态需要重置

    switch(task->task){
        case Task_In:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(800) == 1){
                task->task = Task_Straight;
                straight_count = 0;
                need_reset = 1;  // 下个状态需要重置
            }
            break;

        case Task_Straight:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(straight_count == 0){
                if(BangBang_Straight_mm(1500) == 1){
                    straight_count++;
                    task->task = Task_Spin_Left;
                    need_reset = 1;
                }
            }
            else if(straight_count == 1){
                if(BangBang_Straight_mm(350) == 1){
                    straight_count++;
                    task->task = Task_Spin_Right;
                    need_reset = 1;
                }
            }
            else {
                task->task = Task_Over;  // 完成
            }
            break;

        case Task_Spin_Left:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Spin_angle(Spin_Mode_Edge, 90) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            }
            break;

        case Task_Spin_Right:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Spin_angle(Spin_Mode_Edge, -90) == 1){
                task->task = Task_Out;
                need_reset = 1;
            }
            break;

        case Task_Out:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(350) == 1){
                task->task = Task_Over;
                need_reset = 1;
            }
            break;

        case Task_Over:
            // Car_Reset(1);
            // straight_count = 0;
            // need_reset = 0;
            break;

        case Task_Turn_Left:
            break;
        case Task_Turn_Right:
            // TODO: 实现转向逻辑
            break;
    }
}

void Task2_StateMachine(Task_t *task){
    static uint8_t straight_count = 0;
    static uint8_t left_count = 0;

    static uint8_t need_reset = 1;  // 表示当前状态需要重置

    switch(task->task){
        case Task_In:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(1200) == 1){
                task->task = Task_Spin_Left;
                straight_count = 0;
                need_reset = 1;  // 下个状态需要重置
            }
            break;

        case Task_Straight:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(straight_count == 0){
                if(BangBang_Straight_mm(705) == 1){
                    straight_count++;
                    task->task = Task_Spin_Right;
                    need_reset = 1;
                }
            }else if(straight_count == 1){
                if(BangBang_Straight_mm(800) == 1){
                    straight_count++;
                    task->task = Task_Spin_Left;
                    need_reset = 1;
                }
            }else{
                if(BangBang_Straight_mm(1205) == 1){
                    straight_count++;
                    task->task = Task_Over;
                    need_reset = 1;
                }
            }
            break;

        case Task_Spin_Left:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(left_count == 0){
                if(BangBang_Spin_angle(Spin_Mode_Center, 45) == 1){
                    left_count++;
                    task->task = Task_Straight;
                    need_reset = 1;
                }
            }else if(left_count == 1){
                if(BangBang_Spin_angle(Spin_Mode_Center, 90) == 1){
                    left_count++;
                    task->task = Task_Straight;
                    need_reset = 1;
                }
            }
            break;

        case Task_Spin_Right:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Spin_angle(Spin_Mode_Center, -90) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            }
            break;

        case Task_Out:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(350) == 1){
                task->task = Task_Over;
                need_reset = 1;
            }
            break;

        case Task_Over:
            // Car_Reset(1);
            // straight_count = 0;
            // need_reset = 0;
            break;

        case Task_Turn_Left:
            break;
        case Task_Turn_Right:
            break;
    }
}

void Task3_StateMachine(Task_t *task){
    static uint8_t straight_count = 0;

    static uint8_t need_reset = 1;  // 表示当前状态需要重置

    switch(task->task){
        case Task_In:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(1250) == 1){
                task->task = Task_Turn_Right;
                straight_count = 0;
                need_reset = 1;  // 下个状态需要重置
            }
            break;

        case Task_Straight:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(straight_count == 0){
                if(BangBang_Straight_mm(325) == 1){
                    straight_count++;
                    task->task = Task_Turn_Left;
                    need_reset = 1;
                }
            }else if(straight_count == 1){
                if(BangBang_Straight_mm(550) == 1){
                    straight_count++;
                    task->task = Task_Spin_Left;
                    need_reset = 1;
                }
            }else{
                if(BangBang_Straight_mm(1205) == 1){
                    straight_count++;
                    task->task = Task_Over;
                    need_reset = 1;
                }
            }
            break;

        case Task_Spin_Left:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Spin_angle(Spin_Mode_Center, 36) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            break;
        }
        case Task_Spin_Right:
            break;

        case Task_Out:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(350) == 1){
                task->task = Task_Over;
                need_reset = 1;
            }
            break;

        case Task_Over:
            // Car_Reset(1);
            // straight_count = 0;
            // need_reset = 0;
            break;

        case Task_Turn_Left:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Revolve_angle(360) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            }
            break;
        case Task_Turn_Right:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Revolve_angle(-360) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            }
            break;
    }
}

void Task4_StateMachine(Task_t *task){
    static uint8_t straight_count = 0;
    static uint8_t need_reset = 1;  // 表示当前状态需要重置

    switch(task->task){
        case Task_In:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(800) == 1){
                task->task = Task_Spin_Left;
                straight_count = 0;
                need_reset = 1;  // 下个状态需要重置
            }
            break;

        case Task_Straight:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(straight_count == 0){
                if(BangBang_Straight_mm(900) == 1){
                    straight_count++;
                    task->task = Task_Spin_Right;
                    need_reset = 1;
                }
            }
            else if(straight_count == 1){
                if(BangBang_Straight_mm(1000) == 1){
                    straight_count++;
                    task->task = Task_Spin_Right;
                    need_reset = 1;
                }
            }else if(straight_count == 2){
                if(BangBang_Straight_mm(200) == 1){
                    straight_count++;
                    task->task = Task_Spin_Left;
                    need_reset = 1;
                }
            }
            else {
                if(BangBang_Straight_mm(1000) == 1){
                    task->task = Task_Over;
                    need_reset = 1;
                }
            }
            break;

        case Task_Spin_Left:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Spin_angle(Spin_Mode_Edge, 90) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            }
            break;

        case Task_Spin_Right:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Spin_angle(Spin_Mode_Edge, -90) == 1){
                task->task = Task_Straight;
                need_reset = 1;
            }
            break;

        case Task_Out:
            if(need_reset){
                Car_Reset(1);
                need_reset = 0;
            }
            if(BangBang_Straight_mm(350) == 1){
                task->task = Task_Over;
                need_reset = 1;
            }
            break;

        case Task_Over:
            // Car_Reset(1);
            // straight_count = 0;
            // need_reset = 0;
            break;

        case Task_Turn_Left:
            break;
        case Task_Turn_Right:
            // TODO: 实现转向逻辑
            break;
    }
}
