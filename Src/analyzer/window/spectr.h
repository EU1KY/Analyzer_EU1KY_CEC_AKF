#ifndef SPECTR_H_
#define SPECTR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    float CalcBin107(uint32_t frequency, int iterations, float *, float *, float *);
    void SPECTR_FindFreq(void); // ** WK **
    extern float value1, value2, value3;
#endif
