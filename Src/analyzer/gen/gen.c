/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   Modified by KD8CEC for CLK2
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */
#include "si5351.h"
#include "adf4350.h"
#include "adf4351.h"
#include "si5338a.h"
#include "gen.h"
#include "dsp.h"
#include "config.h"
#include "stm32746g_discovery_audio.h"
#include "crash.h"

static uint32_t lastSetFreq = 14000000ul;
extern void Sleep(uint32_t);

//Generator driver descriptor
typedef struct
{
    void (*Init)(void);      //Initialize generator
    void (*Off)(void);       //Shutdown generator
    void (*SetF0)(uint32_t); //Set F0 frequency (i.e. measurement frequency)
    void (*SetLO)(uint32_t); //Set LO frequency (i.e. F0 + IF)
    void (*SetTX)(uint32_t); //Set TX frequency (i.e. measurement frequency)
} GenDrv_t;

static GenDrv_t gen = {0}; //Generator driver descriptor

void GEN_Init(void)
{
    //available just Si5351
    gen.Init = si5351_Init;
    gen.Off = si5351_Off;
    gen.SetF0 = si5351_SetF0;
    gen.SetLO = si5351_SetLO;
    gen.SetTX = si5351_SetF2;

    //below comment by EU1KY
    //Set pointers to generator driver functions
    //Todo: add other variants using CFG_PARAM_SYNTH_TYPE
    /*
    if (CFG_SYNTH_SI5351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        gen.Init = si5351_Init;
        gen.Off = si5351_Off;
        gen.SetF0 = si5351_SetF0;
        gen.SetLO = si5351_SetLO;
        gen.SetTX = si5351_SetF2;
    }
    else if (CFG_SYNTH_ADF4350 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        gen.Init = adf4350_Init;
        gen.Off = adf4350_Off;
        gen.SetF0 = adf4350_SetF0;
        gen.SetLO = adf4350_SetLO;
    }
    else if (CFG_SYNTH_ADF4351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        gen.Init = ADF4351_Init;
        gen.Off = ADF4351_Off;
        gen.SetF0 = ADF4351_SetF0;
        gen.SetLO = ADF4351_SetLO;
    }
    else if (CFG_SYNTH_SI5338A == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        gen.Init = SI5338A_Init;
        gen.Off = SI5338A_Off;
        gen.SetF0 = SI5338A_SetF0;
        gen.SetLO = SI5338A_SetLO;
    }
    else
    {
        CRASH("Unexpected frequency synthesizer type in configuration");
    }
    */

    //Initialize generator
    gen.Init();
}

void GEN_SetMeasurementFreq(uint32_t fhz)
{
    uint32_t IF = DSP_GetIF(); //Required IF frequency
    if (fhz == 0)
    {
        gen.Off();
        return;
    }

    if (CFG_SYNTH_SI5351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        if (fhz > 3 * CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        {                              //test!!
            gen.SetF0(fhz / 5);        //Set F0 on 5th harmonic
            gen.SetLO((fhz - IF) / 5); // ** WK **
        }
        else if (fhz > CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        {
            gen.SetF0(fhz / 3);        //Set F0 on 3rd harmonic
            gen.SetLO((fhz - IF) / 3); // ** WK **
        }                              // made problems, if fhz == CFG_PARAM_SI5351_MAX_FREQ
        else
        {
            gen.SetF0(fhz);
            gen.SetLO(fhz - IF);
        }
    }
    else
    {
        gen.SetF0(fhz);
        gen.SetLO(fhz - IF);
    }

    lastSetFreq = fhz;
    Sleep(0); // was 2 WK
}

//KD8CEC
void GEN_SetTXFreq(uint32_t fhz)
{
    uint32_t IF = DSP_GetIF(); //Required IF frequency
    if (fhz == 0)
    {
        gen.Off();
        return;
    }

    if (CFG_SYNTH_SI5351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        if (fhz > 3 * CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        {                              //test!!
            gen.SetTX(fhz / 5);        //Set F0 on 5th harmonic
            gen.SetLO((fhz - IF) / 5); // ** WK **
        }
        else if (fhz > CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        {
            gen.SetTX(fhz / 3);        //Set F0 on 3rd harmonic
            gen.SetLO((fhz - IF) / 3); // ** WK **
        }                              // made problems, if fhz == CFG_PARAM_SI5351_MAX_FREQ
        else
        {
            gen.SetTX(fhz);
            gen.SetLO(fhz - IF);
        }
    }
    else
    {
        gen.SetTX(fhz);
        gen.SetLO(fhz - IF);
    }

    lastSetFreq = fhz;
    //Sleep(0);// was 2 WK
}

void GEN_SetLOFreq(uint32_t frqu1)
{ // ** WK ** 02.04.2018

    if (frqu1 > 3 * CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        gen.SetLO(frqu1 / 5);
    else if (frqu1 > CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        gen.SetLO(frqu1 / 3);
    else
        gen.SetLO(frqu1);
}

void GEN_SetF0Freq(uint32_t frqu1)
{ // ** WK ** 02.04.2018
    if (frqu1 == 0)
    {
        gen.Off();
        return;
    }

    if (frqu1 > 3 * CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        gen.SetF0(frqu1 / 5);

    else if (frqu1 > CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        gen.SetF0(frqu1 / 3);
    else
        gen.SetF0(frqu1);
}

void GEN_SetClk2Freq(uint32_t frqu1)
{ // ** WK ** 22.10.2020
    if (frqu1 == 0)
    {
        gen.Off();
        return;
    }

    if (frqu1 > 3 * CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        gen.SetTX(frqu1 / 5);

    else if (frqu1 > CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ))
        gen.SetTX(frqu1 / 3);
    else
        gen.SetTX(frqu1);
}

uint32_t GEN_GetLastFreq()
{
    return lastSetFreq;
}
