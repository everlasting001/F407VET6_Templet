#ifndef __MOVE_CONTROL_H__
#define __MOVE_CONTROL_H__

#include "Variable.h"
#include "bsp_headers.h"
#include "Config.h"
#include <stdint.h>

#define Spin_Mode_Edge 0
#define Spin_Mode_Center 1

void Car_Reset(uint8_t flag);
uint8_t BangBang_Straight_mm(float distance_mm);
uint8_t BangBang_Spin_angle(uint8_t mode,float target_angle);
uint8_t BangBang_Revolve_angle(float target_angle);

#endif
