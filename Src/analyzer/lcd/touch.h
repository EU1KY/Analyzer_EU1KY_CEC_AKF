/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#ifndef _TOUCH_H_
#define _TOUCH_H_

#include "LCD.h"
#include "BeepTimer.h"

#ifdef __cplusplus
extern "C" {
#endif

void TOUCH_Init(void);

uint8_t TOUCH_Poll(LCDPoint* pCoord);

uint8_t TOUCH_IsPressed(void);

//KD8CEC
int GetTouchIndex(LCDPoint pt, const int checkButtons[][4], int checkCount);

#define BUTTON_LEFT     0
#define BUTTON_TOP      1
#define BUTTON_RIGHT    2
#define BUTTON_BOTTOM   3

#ifdef __cplusplus
}
#endif
#endif //_TOUCH_H_

