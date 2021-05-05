#ifndef GEN_H_
#define GEN_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void GEN_Init(void);
    void GEN_SetMeasurementFreq(uint32_t fhz);
    uint32_t GEN_GetLastFreq(void);
    void GEN_SetLOFreq(uint32_t frqu1); // ** WK **
    void GEN_SetF0Freq(uint32_t frqu1); // ** WK **
    void GEN_SetClk2Freq(uint32_t frqu1);
    void GEN_SetTXFreq(uint32_t fhz);

#ifdef __cplusplus
}
#endif

#endif
