#ifndef FREQCOUNTER_H_
#define FREQCOUNTER_H_

extern int BeepIsActive, SWRTone;

void UB_TIMER2_Init_FRQ(uint32_t frq_hz);
void UB_TIMER2_Start();
void UB_TIMER2_Stop();
void InitTimer2_4_5(void);

#endif
