/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#ifndef _MAINWND_H_
#define _MAINWND_H_

#include <ctype.h>
#include "main.h"

void MainWnd(void);
extern uint16_t FileNo;
extern volatile int Page;
extern uint32_t date, time;
extern uint32_t RTCpresent;
extern volatile int NoDate;
extern ADC_HandleTypeDef Adc3Handle;
#endif
