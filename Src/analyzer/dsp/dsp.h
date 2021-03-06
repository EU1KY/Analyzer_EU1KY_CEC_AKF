/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#ifndef DSP_H_INCLUDED
#define DSP_H_INCLUDED

#include <stdint.h>
#include <complex.h>
#include "stm32746g_discovery_audio.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef float complex DSP_RX;

    void DSP_Init(void);

    void DSP_Measure(uint32_t freqHz, int applyErrCorr, int applyOSL, int nMeasurements);
    void DSP_Measure2(void);

    DSP_RX DSP_MeasuredZ(void);
    float DSP_MeasuredPhase(void);
    float DSP_MeasuredDiff(void);
    float DSP_MeasuredDiffdB(void);
    float DSP_MeasuredPhaseDeg(void);
    float DSP_MeasuredMagVmv(void);
    float DSP_MeasuredMagImv(void);
    float complex DSP_MeasuredMagPhaseV(void);
    float complex DSP_MeasuredMagPhaseI(void);
    float DSP_CalcX(void);

    float DSP_CalcVSWR(DSP_RX Z);
    uint32_t DSP_GetIF(void);
    void DSP_Sample(void);
    void DSP_Sample16(void);
    float DSP_GetValue(uint32_t frequency, int iterations);

    //KD8CEC
    float DSP_MeasureTrack(uint32_t freqHz, int applyErrCorr, int applyOSL, int nMeasurements);
    float DSP_MeasuredTrackValue();
    float DSP_MeasureTrackCal(void);

#define NSAMPLES 512              //Must be order of 2
#define NDUMMY 32                 //Dummy samples are needed to minimize influence of filter settling after invoking the SAI
#define FSAMPLE I2S_AUDIOFREQ_48K //Sampling frequency
#define FFTBIN 107                //Bin 107 determines 10031 Hz intermediate frequency at 512 samples at 48 kHz.

//Magnitude correction factor @ max linear input gain
#define MCF 0.013077f

#if (FFTBIN >= ((NSAMPLES) / 2))
#error FFT bin is selected incorrectly
#endif

#ifdef __cplusplus
}
#endif

#endif //DSP_H_INCLUDED
