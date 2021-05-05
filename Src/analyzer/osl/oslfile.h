#ifndef _OSLFILE_H_
#define _OSLFILE_H_

#include <complex.h>
#include <stdint.h>
#include "config.h"

typedef union
{
    struct
    {
        float mag0;   //Magnitude ratio correction coefficient
        float phase0; //Phase correction value
    };
    struct
    {
        float val0;
        float valAtt;
    };
} OSL_ERRCORR;

//OSL calibration data structure
typedef union
{
    struct
    {
        float complex e00; //e00 correction coefficient
        float complex e11; //e11 correction coefficient
        float complex de;  //delta-e correction coefficient
    };
    struct
    {
        float complex gshort; //measured gamma for short load
        float complex gload;  //measured gamma for 50 Ohm load
        float complex gopen;  //measured gamma for open load
    };
} S_OSLDATA;

OSL_ERRCORR *osl_txCorr;
float *WORK_Ptr;

float complex OSL_GFromZ(float complex Z, float Rbase);
float complex OSL_ZFromG(float complex Z, float Rbase);
float complex OSL_CorrectZ(uint32_t fhz, float complex zMeasured);
float complex OSL_GtoMA(float complex G);
float complex OSL_ParabolicInterpolation(float complex y1, float complex y2, float complex y3, //values for frequencies x1, x2, x3
                                         float x1, float x2, float x3,                         //frequencies of respective y values
                                         float x);                                             //Frequency between x2 and x3 where we want to interpolate result

int32_t OSL_GetSelected(void);
const char *OSL_GetSelectedName(void);
void OSL_Select(int32_t index);
int32_t OSL_IsSelectedValid(void);

void OSL_ScanOpen(void (*progresscb)(uint32_t));
void OSL_ScanShort(void (*progresscb)(uint32_t));
void OSL_ScanLoad(void (*progresscb)(uint32_t));
void OSL_Calculate(void);
void OSL_LoadErrCorr(void);
void OSL_ScanErrCorr(void (*progresscb)(uint32_t));
void OSL_CorrectErr(uint32_t fhz, float *magdif, float *phdif);
int32_t OSL_IsErrCorrLoaded(void);
int Get_OSL_Entries(void);
//KD8CEC
void OSL_LoadTXCorr(void);
void OSL_ScanTXCorr(void (*progresscb)(uint32_t));
void OSL_ScanTXAttenuator(void (*progresscb)(uint32_t));
float OSL_CorrectTX(float inp, float Corr0dB, float CorrAtt);
int32_t OSL_IsTXCorrLoaded(void);
int OSL_Calc_dBPix(float inp);
extern void progress_cb(uint32_t new_percent);
extern void S21progress_cb(uint32_t new_percent);
void SaveS21CorrToFile(void);
uint32_t OSL_GetCalFreqByIdx(int32_t idx);
int GetIndexForFreq(uint32_t fhz);
void SmoothOSL(void);

#endif //_OSLFILE_H_
