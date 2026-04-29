#include "Key.h"

#define KEY_PRESS 0x01
#define KEY_RELEASE 0x00

#define KEY_REPEAT_TIME 125
#define KEY_DOUBLE_TIME 250
#define KEY_LONG_PRESS_TIME 2000

uint8_t Key_Flag[BUTTLE_COUNT];
/**
  * 函    数：获取按键状态
  * 参    数：无
  * 返 回 值：有按键按下，直接返回键码（非阻塞），没有按键按下，返回0
  */
//Button:1,2,3,4
uint8_t Key_GetState(uint8_t Button){
	Button = Button - 1;
	switch(Button){
		case 0:
			if(HAL_GPIO_ReadPin(Key1_GPIO_Port, Key1_Pin) == GPIO_PIN_RESET){
				return KEY_PRESS;
			}
			return KEY_RELEASE;
		case 1:
			if(HAL_GPIO_ReadPin(Key2_GPIO_Port, Key2_Pin) == GPIO_PIN_RESET){
				return KEY_PRESS;
			}
			return KEY_RELEASE;
		// case 2:
		// 	if(HAL_GPIO_ReadPin(Key3_GPIO_Port, Key3_Pin) == GPIO_PIN_RESET){
		// 		return KEY_PRESS;
		// 	}
		// 	return KEY_RELEASE;
		// case 3:
		// 	if(HAL_GPIO_ReadPin(Key4_GPIO_Port, Key4_Pin) == GPIO_PIN_RESET){
		// 		return KEY_PRESS;
		// 	}
		// 	return KEY_RELEASE;
		default:
			return 0;
	}
}

/**
  * 函    数：检查按键以何种方式按下
  * 参    数：Button：按键编号，Flag：按键状态标志位
  * 返 回 值：按键是否按下，1：是，0：否
  */
uint8_t Key_Check(uint8_t Button,uint8_t Flag){
	Button = Button - 1;
	if(Key_Flag[Button] & Flag){
		if(Flag != KEY_HOLD){
			Key_Flag[Button] &= ~Flag;
		}
		return 1;
	}
	return 0;
}

/**
  * 函    数：清除按键标志位
  * 参    数：无
  * 返 回 值：无
  * 注意事项：切换模式时，需要清除标志位，否则会导致误判
  */
void Key_ClearFlag(void){
	for(int i=0; i<BUTTLE_COUNT; i++){
		Key_Flag[i] = 0x00;
	}
}

/**
  * 函    数：用于驱动按键模块运行的自定义按键定时中断函数
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数必须在主程序中每隔1ms自动执行一次
  */
void Key_Tick(void)
{
	/*定义静态变量（默认初值为0，函数退出后保留值和存储空间）*/
	static uint8_t Cnt,i;					
	static uint8_t CurrState[BUTTLE_COUNT], PrevState[BUTTLE_COUNT];	
	static uint8_t State[BUTTLE_COUNT];
	static uint16_t TimeCnt[BUTTLE_COUNT];
	Cnt ++;			
	if (Cnt >= 20)	//定时器每20ms进一次
	{
		Cnt = 0;
	}
	for(i=0; i<BUTTLE_COUNT; i++){
		if(TimeCnt[i] > 0){
			TimeCnt[i]--;
		}		
		
		PrevState[i] = CurrState[i];			
		CurrState[i] = Key_GetState(i+1);		

		if(CurrState[i] == KEY_PRESS){
			Key_Flag[i] |= KEY_HOLD;
		}
		else{
			Key_Flag[i] &= ~KEY_HOLD;
		}

		if(CurrState[i] == KEY_PRESS && PrevState[i] == KEY_RELEASE){
			Key_Flag[i] |= KEY_DOWN;
		}

		if(CurrState[i] == KEY_RELEASE && PrevState[i] == KEY_PRESS){
			Key_Flag[i] |= KEY_UP;
		}

		switch(State[i]){
			case 0:	//初始状态
				if(CurrState[i] == KEY_PRESS){
					TimeCnt[i] = KEY_LONG_PRESS_TIME;
					State[i] = 1;
				}
				break;
			case 1:	//按键按下状态，等待长按时间和松开
				if(CurrState[i] == KEY_RELEASE){
					TimeCnt[i] = KEY_DOUBLE_TIME;
					State[i] = 2;
				}else if(TimeCnt[i] == 0){
					TimeCnt[i] = KEY_REPEAT_TIME;
					Key_Flag[i] |= KEY_LONG_PRESS;
					State[i] = 4;
				}
				break;
			case 2: //按键释放状态，等待双击
				if(CurrState[i] == KEY_PRESS){
					Key_Flag[i] |= KEY_DOUBLE;
					State[i] = 3;
				}else if(TimeCnt[i] == 0){
					Key_Flag[i] |= KEY_SINGLE;
					State[i] = 0;
				}
				break;
			case 3: //按键双击状态
				if(CurrState[i] == KEY_RELEASE){
					State[i] = 0;	
				}
				break;
			case 4: //按键长按状态
				Key_Flag[i] |= KEY_LONG_PRESS;
				if(CurrState[i] == KEY_RELEASE){
					State[i] = 0;
				}else if(TimeCnt[i] == 0){
					TimeCnt[i] = KEY_REPEAT_TIME;
					Key_Flag[i] |= KEY_REPEAT;
				}
				break;
		}
	}
}