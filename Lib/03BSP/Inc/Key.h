#ifndef __KEY_H__
#define __KEY_H__

#include "main.h"
#include <stdint.h>
#include "Variable.h"

#define KEY_HOLD 0x01
#define KEY_DOWN 0x02
#define KEY_UP 0x04
#define KEY_SINGLE 0x08
#define KEY_DOUBLE 0x10
#define KEY_LONG_PRESS 0x20
#define KEY_REPEAT 0x40
#define BUTTLE_COUNT 4
#define Key1 1
#define Key2 2
#define Key3 3
#define Key4 4

uint8_t Key_GetState(uint8_t Button);
void Key_Tick(void);
uint8_t Key_Check(uint8_t Button,uint8_t Flag);
void Key_ClearFlag(void);

#endif
