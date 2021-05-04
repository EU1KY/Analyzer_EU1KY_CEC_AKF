/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#include <stdint.h>
#include <stdio.h>
#include "LCD.h"
#include "dsp.h"
#include "font.h"
#include "touch.h"
#include "gen.h"
#include "config.h"
#include "hit.h"
#include "main.h"
#include "measurement.h"
#include "num_keypad.h"
#include "panfreq.h"
#include "panvswr2.h"
#include "generator.h"
#include "si5351.h"
#include "si5351_hs.h"
#include "FreqCounter.h"

extern uint32_t BackGrColor;
extern uint32_t TextColor;
extern uint16_t TimeFlag;

static uint32_t fChanged = 0;
static uint32_t rqExit = 0;
static uint32_t f_maxstep = 500000;
static uint32_t redrawWindow;
static uint32_t fx = 14000000ul; //start frequency, in Hz
static uint32_t fxkHz;//frequency, in kHz
static BANDSPAN *pBs1;
static LCDPoint pt;
static bool mod_AM, mod_FM ;
static uint32_t fxG;
void Sleep(uint32_t ms);

uint32_t S2powerDBm[4]={5,10,12,14}; //rounded output values at 2 MHz

static void ShowF()
{
char str[20] = "";
uint8_t i,j;
    FONT_ClearHalfLine(FONT_FRANBIG, BackGrColor, 72);// WK
    sprintf(str, "F: %u MHz", (unsigned int)(CFG_GetParam(CFG_PARAM_GEN_F) ));
    for(i=3;i<15;i++){
        if(str[i]==' ') break;// search space before "MHz"
    }
    for(j=i+4;j>i-4;j--){//4
       str[j+2]=str[j];
    }
     for(j=i-4;j>i-7;j--){//7
       str[j+1]=str[j];
    }
    str[i-6]='.';
    str[i-2]=' ';
    str[i+6]=0;
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 0, 72, str );
}

void FDecrG(uint32_t step)
{
    uint32_t MeasurementFreq = CFG_GetParam(CFG_PARAM_GEN_F);
    MeasurementFreq -= step;
    if(MeasurementFreq < CFG_GetParam(CFG_PARAM_BAND_FMIN)) MeasurementFreq=CFG_GetParam(CFG_PARAM_BAND_FMIN);;
    CFG_SetParam(CFG_PARAM_GEN_F, MeasurementFreq);
    fChanged = 1;
    freqMHzf=MeasurementFreq/1000000.;
    fxG=MeasurementFreq;
    Sleep(50);
}

 void FIncrG(uint32_t step)
{
    uint32_t MeasurementFreq = CFG_GetParam(CFG_PARAM_GEN_F);
    MeasurementFreq += step;
    if(MeasurementFreq > CFG_GetParam(CFG_PARAM_BAND_FMAX)) MeasurementFreq=CFG_GetParam(CFG_PARAM_BAND_FMAX);;
    CFG_SetParam(CFG_PARAM_GEN_F, MeasurementFreq);
    fChanged = 1;
    freqMHzf=MeasurementFreq/1000000.;
    fxG=MeasurementFreq;
    Sleep(50);
}


static void GENERATOR_Exit(void)
{
    rqExit = 1;
}

static void GENERATOR_FDecr_500k(void)
{
    FDecrG(f_maxstep);
}
static void GENERATOR_FDecr_100k(void)
{
    FDecrG(100000);
}
static void GENERATOR_FDecr_10k(void)
{
    FDecrG(10000);
}
static void GENERATOR_FDecr_1k(void)
{
    FDecrG(1000);
}
static void GENERATOR_FDecr_100Hz(void)// WK
{
    FDecrG(100);
}
static void GENERATOR_FDecr_10Hz(void)// WK
{
    FDecrG(10);
}

static void GENERATOR_FIncr_10Hz(void)
{
    FIncrG(10);
}

static void GENERATOR_FIncr_100Hz(void)
{
    FIncrG(100);
}

static void GENERATOR_FIncr_1k(void)
{
    FIncrG(1000);
}
static void GENERATOR_FIncr_10k(void)
{
    FIncrG(10000);
}
static void GENERATOR_FIncr_100k(void)
{
    FIncrG(100000);
}
static void GENERATOR_FIncr_500k(void)
{
    FIncrG(f_maxstep);
}


static void GENERATOR_SetFreq(void)
{
//    while(TOUCH_IsPressed()); WK
    fx=CFG_GetParam(CFG_PARAM_GEN_F);
    fxkHz=fx/1000;
    if (PanFreqWindow(&fxkHz, (BANDSPAN*)&pBs1))
    {
        //Span or frequency has been changed
        CFG_SetParam(CFG_PARAM_GEN_F, fxkHz*1000);
    }
   // uint32_t val = NumKeypad(CFG_GetParam(CFG_PARAM_MEAS_F)/1000, BAND_FMIN/1000, CFG_GetParam(CFG_PARAM_BAND_FMAX)/1000, "Set measurement frequency, kHz");
    CFG_Flush();
    redrawWindow = 1;
    Sleep(200);
}


void GENERATOR_AM(void){
int k=0;
    while(TOUCH_IsPressed());
    Sleep(100);
    fx=CFG_GetParam(CFG_PARAM_GEN_F);
    if(mod_AM==false){
        mod_AM=true;
        GEN_SetMeasurementFreq(fx);
        GEN_SetClk2Freq(fx);
        HS_SetPower(2, CLK2_drive, 1);// CLK2: 8 mA .. 2 mA
        HS_SetPower(0, CLK2_drive, 1);// CLK0: 8 mA .. 2 mA
        LCD_Rectangle((LCDPoint){320,234},(LCDPoint){379,271}, 0xffff0000);//red
        LCD_Rectangle((LCDPoint){319,233},(LCDPoint){378,270}, 0xffff0000);//red
        for(;;){
            k++;
            if(k>=1000)  k=0;
            if(k%2==1){
                GEN_SetMeasurementFreq(0);
                GEN_SetClk2Freq(0);
                //HS_SetPower(2, 0, 1);// CLK2: 2 mA
                //HS_SetPower(0, 0, 1);// CLK0: 2 mA
                Sleep(1);
            }
            else{
                GEN_SetMeasurementFreq(fx);
                GEN_SetClk2Freq(fx);
                //HS_SetPower(2, 3, 1);// CLK2: 8 mA
                //HS_SetPower(0, 3, 1);// CLK0: 8 mA
                Sleep(2);
            }
            if(k%20==1){
                if (TOUCH_Poll(&pt))  {
                    break;
                }
            }
        }
    }
    mod_AM=false;
    redrawWindow=1;
    Sleep(100);
    return;
}

void GENERATOR_FM(void){
int k=0;
    while(TOUCH_IsPressed());
    Sleep(100);
    fx=CFG_GetParam(CFG_PARAM_GEN_F);
    if(mod_FM==false){
        mod_FM=true;
        LCD_Rectangle((LCDPoint){380,234},(LCDPoint){440,271}, 0xffff0000);//red
        LCD_Rectangle((LCDPoint){379,233},(LCDPoint){439,270}, 0xffff0000);//red
        HS_SetPower(2, CLK2_drive, 1);// CLK2: 8 mA .. 2 mA
        HS_SetPower(0, CLK2_drive, 1);// CLK0: 8 mA .. 2 mA
        for(;;){
            Sleep(2);
            k++;
            if(k%2==1){
                GEN_SetMeasurementFreq(fx-500);
                GEN_SetClk2Freq(fx-500);
            }
            else{
                GEN_SetMeasurementFreq(fx+500);
                GEN_SetClk2Freq(fx+500);
            }
            if(k==1000) k=0;
            if(k%5==1){
                if (TOUCH_Poll(&pt))  {
                    break;
                }
            }
        }
    }
    mod_FM=false;
    GEN_SetMeasurementFreq(fx);
    GEN_SetClk2Freq(fx);
    redrawWindow=1;


}

static void GENERATOR_Power(void){
   CLK2_drive++;
   if( CLK2_drive>3) CLK2_drive=0;
   Sleep(1);
   Beep(1);
   redrawWindow=1;
   Sleep(100);
}

static const struct HitRect GENERATOR_hitArr[] =
{
    //        x0,  y0, width, height, callback
    HITRECT(   0, 233,  65, 38, GENERATOR_Exit),//200
    HITRECT(  70, 233, 125, 38, GENERATOR_SetFreq),
    HITRECT( 200, 233, 80, 38, GENERATOR_Power),
    HITRECT( 320, 233, 59, 38, GENERATOR_AM),
    HITRECT( 380, 233, 59, 38, GENERATOR_FM),
    HITRECT( 290,   1,  90, 35, GENERATOR_FDecr_500k),
    HITRECT( 290,  40,  90, 35, GENERATOR_FDecr_100k),
    HITRECT( 290,  79,  90, 35, GENERATOR_FDecr_10k),
    HITRECT( 290, 118,  90, 35, GENERATOR_FDecr_1k),
    HITRECT( 290, 157,  90, 35, GENERATOR_FDecr_100Hz),
    HITRECT( 290, 196,  90, 35, GENERATOR_FDecr_10Hz),
    HITRECT( 380, 196,  90, 35, GENERATOR_FIncr_10Hz),
    HITRECT( 380, 157,  90, 35, GENERATOR_FIncr_100Hz),
    HITRECT( 380, 118,  90, 35, GENERATOR_FIncr_1k),
    HITRECT( 380,  79,  90, 35, GENERATOR_FIncr_10k),
    HITRECT( 380,  40,  90, 35, GENERATOR_FIncr_100k),
    HITRECT( 380,   1,  90, 35, GENERATOR_FIncr_500k),
    HITEND
};
void ShowIncDec1(void){
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,300,   2, "-0.5M");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,300,  41, "-0.1M");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,300,  80, "-10k");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,300, 119, "-1k");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,300, 158, "-0.1k");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,300, 197, "-10Hz");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,380, 197, "+10Hz");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,380, 158, "+0.1k");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,380, 119, "+1k");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,380,  80, "+10k");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,380,  41, "+0.1M");
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor,380,   2, "+0.5M");
}

static DSP_RX rx;


//added wk 21.1.2019
//not used yet by kd8cec (wk used check si5351 signal in main.c)
int testGen(void){
    GEN_SetMeasurementFreq(3500000ul);
    Sleep(200);
    DSP_Measure(0, 1, 0, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
    if (DSP_MeasuredMagVmv() > 0.3f) return 0;
    return -1;
}

void GENERATOR_Window_Proc(void)
{
int k, SignalGood;
uint32_t speedcnt = 0;

    CLK2_drive=3;// 8 mA
    mod_AM = false;
    mod_FM = false;
    while(TOUCH_IsPressed());
    SetColours();
    {
        uint32_t fbkup = CFG_GetParam(CFG_PARAM_GEN_F);
        if (!(fbkup >= BAND_FMIN && fbkup <= CFG_GetParam(CFG_PARAM_BAND_FMAX) && (fbkup % 1000) == 0))
        {
            CFG_SetParam(CFG_PARAM_GEN_F, 14000000);
            CFG_Flush();
        }
    }
    redrawWindow = 0;
GENERATOR_REDRAW:
    k=99;
    SignalGood=0;
    GEN_SetMeasurementFreq(CFG_GetParam(CFG_PARAM_GEN_F));
    GEN_SetClk2Freq(CFG_GetParam(CFG_PARAM_GEN_F));// ***********

    LCD_FillAll(BackGrColor);
    ShowHitRect(GENERATOR_hitArr);
    ShowIncDec1();
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 0, 36, "Generator mode");// WK
    FONT_Write(FONT_FRANBIG, CurvColor, BackGrColor, 2, 235, " Exit ");
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 205, 235, "Power");
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 75, 235, "Frequency");
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 332, 235, "AM");
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 392, 235, "FM");
    ShowF();
    if(mod_AM==false){
        LCD_Rectangle((LCDPoint){320,234},(LCDPoint){379,271}, 0xffffffff);//white  AM
        LCD_Rectangle((LCDPoint){319,233},(LCDPoint){378,270}, 0xffffffff);//white
    }
    if(mod_FM==false){
        LCD_Rectangle((LCDPoint){380,234},(LCDPoint){440,271}, 0xffffffff);//white FM
        LCD_Rectangle((LCDPoint){379,233},(LCDPoint){439,270}, 0xffffffff);//white
    }

    f_maxstep = 500000;
    rqExit = 0;
    fChanged = 1;// WK
    redrawWindow = 0;
    while(1)
    {
        if(TOUCH_Poll(&pt)){
            if(HitTest(GENERATOR_hitArr, pt.x, pt.y)==1){
                if (rqExit)
                {
                    CLK2_drive=3;// 8 mA
                    GEN_SetMeasurementFreq(0);
                    GEN_SetClk2Freq(0);
                    return; // Back to main menu
                }

                speedcnt++;
                if (speedcnt < 5)
                    Sleep(500);// WK
                else
                    Sleep(200);// WK
                if (speedcnt > 50){
                    speedcnt--;
                    f_maxstep = 2000000;
                }
            }
        }
        if (redrawWindow)
            {
                redrawWindow = 0;
                goto GENERATOR_REDRAW;
            }
        if (fChanged)
        {
            fChanged=0;
            ShowF();
            GEN_SetMeasurementFreq(CFG_GetParam(CFG_PARAM_GEN_F));
            GEN_SetClk2Freq(CFG_GetParam(CFG_PARAM_GEN_F));// ***********
            CFG_Flush();// save all settings
        }
        Sleep(50);
        k++;
        if(k>=100){
            k=0;
            //Measure without changing current frequency and without OSL
            DSP_Measure(0, 1, 0, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));

            FONT_SetAttributes(FONT_FRAN, TextColor, BackGrColor);
            FONT_ClearHalfLine(FONT_FRAN, BackGrColor, 100);
            FONT_Printf(0, 100, "Raw Vi: %.2f mV, Vv: %.2f mV Diff %.2f dB", DSP_MeasuredMagImv(),
                         DSP_MeasuredMagVmv(), DSP_MeasuredDiffdB());
            FONT_ClearHalfLine(FONT_FRAN, BackGrColor, 120);
            FONT_Printf(0, 120, "Raw phase diff: %.1f deg", DSP_MeasuredPhaseDeg());

            rx = DSP_MeasuredZ();
            FONT_ClearHalfLine(FONT_FRAN, BackGrColor, 140);
            FONT_Printf(0, 140, "Raw R: %.1f X: %.1f", crealf(rx), cimagf(rx));
            //if (DSP_MeasuredMagVmv() > 1.f){
            if (DSP_MeasuredMagVmv() > 0.3f){
                if(SignalGood==0){
                    FONT_Write(FONT_FRAN, Color1, BackGrColor, 0, 160, "Signal OK   ");
                    SignalGood=1;
                }
            }
            else{
                if(SignalGood==1){
                    FONT_Write(FONT_FRAN, LCD_RED, BackGrColor, 0, 160,   "No signal  ");
                    SignalGood=0;
                }
            }
            DSP_Measure(0, 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
            rx = DSP_MeasuredZ();
            FONT_ClearHalfLine(FONT_FRAN, BackGrColor, 180);
            FONT_Printf(0, 180, "With OSL R: %.1f X: %.1f", crealf(rx), cimagf(rx));
            FONT_Printf(0,200, "Output Power S2 (approx.:) %2d dBm" , S2powerDBm[CLK2_drive]);
        }
    }
}
