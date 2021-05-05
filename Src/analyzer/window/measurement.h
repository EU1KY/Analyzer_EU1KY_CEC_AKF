/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#ifndef MEASUREMENT_H_
#define MEASUREMENT_H_
extern uint32_t Timer5Value;
extern uint16_t TimeFlag;
void Single_Frequency_Proc(void);
void FIncr(uint32_t step);
void FDecr(uint32_t step);

#endif
