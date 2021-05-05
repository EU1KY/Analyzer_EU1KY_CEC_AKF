/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include "config.h"
#include "LCD.h"
#include "font.h"
#include "touch.h"
#include "hit.h"
#include "textbox.h"
#include "dsp.h"
#include "gen.h"
#include "stm32f746xx.h"
#include "oslfile.h"
#include "stm32746g_discovery_lcd.h"
#include "match.h"
#include "num_keypad.h"
#include "screenshot.h"
#include "smith.h"
#include "measurement.h"
#include "panfreq.h"
#include "panvswr2.h"
#include "bitmaps/bitmaps.h"

extern void Sleep(uint32_t ms);
extern void TRACK_Beep(int duration);
//==============================================================================
static uint8_t MeasRqExit = 0;
static uint8_t MeasRedrawWindow = 0;
static uint8_t fChanged = 0;
static uint8_t isMatch = 0;
static uint32_t meas_maxstep = 500000;
static float vswr500[100];
static float complex zFine500[100] = {0};
static uint8_t DrawFine;
static int parallel;
static unsigned int freqOld = 0;

#define SCAN_ORIGIN_X 20
#define SCAN_ORIGIN_Y 209

uint8_t seconds; // W.Kiefer

static void ShowF()
{
    char str[50];
    uint8_t i, j;
    unsigned int freq = (unsigned int)(CFG_GetParam(CFG_PARAM_MEAS_F) / 1000);
    if (freqOld == freq)
        return;
    freqOld = freq;
    sprintf(str, "F: %u MHz        ", freq);
    if (freq > 999)
    { // WK
        for (i = 3; i < 10; i++)
        {
            if (str[i] == ' ')
                break; // search space before "kHz"
        }
        for (j = i + 3; j > i - 4; j--)
        {
            str[j + 1] = str[j];
        }
        str[i - 3] = '.';
        str[i + 5] = 0;
    }

    LCD_FillRect(LCD_MakePoint(80, 35), LCD_MakePoint(214, 72), BackGrColor);
    FONT_Write(FONT_FRANBIG, LCD_RGB(255, 127, 39), BackGrColor, 0, 36, str); // WK
    GEN_SetMeasurementFreq(CFG_GetParam(CFG_PARAM_MEAS_F));
}

void MeasureFrequency(void)
{
    char strfreq[32];

    if (TimeFlag >= 999)
    { //test, if timer interrupt is working
        TimeFlag = 0;
        seconds++;
        sprintf(strfreq, "Frequ: %u kHz ", (unsigned int)(Timer5Value));
        FONT_Write(FONT_FRAN, TextColor, BackGrColor, 280, 230, strfreq);
        sprintf(strfreq, "Runtime: %d s", seconds);
        FONT_Write(FONT_FRAN, TextColor, BackGrColor, 280, 250, strfreq);
    }
}

void MEASUREMENT_ParSerial(void)
{
    if (parallel == 1)
        parallel = 0;
    else
        parallel = 1;
    MeasRedrawWindow = 1;
    while (TOUCH_IsPressed())
        ;
}

void DrawSmallSmith(int X0, int Y0, int R, float complex rx)
{

    LCD_FillRect(LCD_MakePoint(X0 - R - 20, Y0 - R - 5), LCD_MakePoint(LCD_GetWidth() - 3, LCD_GetHeight() - 5), BackGrColor);
    float complex G = OSL_GFromZ(rx, CFG_GetParam(CFG_PARAM_R0));

    if (isMatch)
    { //Print LC match
        X0 = X0 - R - 20;
        Y0 -= R;
        FONT_Print(FONT_FRAN, LCD_PURPLE, LCD_BLACK, X0, Y0, "  LC match for SRC Z0 = %d Ohm", CFG_GetParam(CFG_PARAM_R0));
        Y0 += FONT_GetHeight(FONT_FRAN) + 4;
        FONT_Write(FONT_FRAN, LCD_WHITE, LCD_BLACK, X0, Y0, "SrcParallel       Serial     LoadParallel");
        Y0 += FONT_GetHeight(FONT_FRAN) + 4;
        MATCH_Calc(X0, Y0, rx);
        return;
    }

    SMITH_DrawGrid(X0, Y0, R, Color2, BackGrColor, SMITH_R50 | SMITH_Y50 | SMITH_R25 | SMITH_R10 | SMITH_R100 | SMITH_R200 | SMITH_J50 | SMITH_J100 | SMITH_J200 | SMITH_J25 | SMITH_J10);
    SMITH_DrawLabels(TextColor, BackGrColor, SMITH_R50 | SMITH_R25 | SMITH_R10 | SMITH_R100 | SMITH_R200 | SMITH_J50 | SMITH_J100 | SMITH_J200 | SMITH_J25 | SMITH_J10);

    //Draw mini-scan points
    uint32_t i;
    int x, y;
    float complex Gx = 0.f + 0.fi;
    //SMITH_ResetStartPoint();// WK

    for (i = 0; i < 100; i++)
    {
        if (TOUCH_IsPressed())
            return;
        if (crealf(zFine500[i]) != 9999.)
        {
            Gx = OSL_GFromZ(zFine500[i], CFG_GetParam(CFG_PARAM_R0));
            SMITH_DrawG(i, Gx, CurvColor);
        }
        else
            SMITH_DrawG(800, Gx, CurvColor); // trick, to write lastoffsets
    }

    x = (int)(crealf(G) * R) + X0;
    y = Y0 - (int)(cimagf(G) * R);
    /*if(LCD_Get_Orientation()==1){
        x=479-x;
        y=271-y;
    }*/
    LCD_FillRect(LCD_MakePoint(x - 3, y - 3), LCD_MakePoint(x + 3, y + 3), CurvColor); // Set Cursor
}
void InitScan500(void)
{
    int l;
    for (l = 0; l < 100; l++)
    {
        zFine500[l] = 9999.f + 0.f * I;
        vswr500[l] = 9999.f;
    }
}

//Scan VSWR in +/- 500 kHz range around measurement frequency with 100 kHz step, to draw a small graph below the measurement
void Scan500(int i, int k)
{
    DSP_RX RX;
    int fq = (int)CFG_GetParam(CFG_PARAM_MEAS_F) + (5 * i + k - 50) * 10000;
    if (fq >= BAND_FMIN) // f >= 100 kHz
    {
        GEN_SetMeasurementFreq(fq);
        Sleep(2); // was 10
        DSP_Measure(fq, 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
        RX = zFine500[5 * i + k] = DSP_MeasuredZ();
        vswr500[5 * i + k] = DSP_CalcVSWR(RX);
    }
    else
    {
        vswr500[5 * i + k] = 9999.f;
    }

    /* if(LCD_Get_Orientation()==1)
      LCD_Line(LCD_MakePoint(479-SCAN_ORIGIN_X, 271-SCAN_ORIGIN_Y-20), LCD_MakePoint(479-SCAN_ORIGIN_X - i * 10, 271-SCAN_ORIGIN_Y-20), LCD_GREEN);
    else*/
    LCD_Line(LCD_MakePoint(SCAN_ORIGIN_X, SCAN_ORIGIN_Y + 20), LCD_MakePoint(SCAN_ORIGIN_X + i * 10, SCAN_ORIGIN_Y + 20), LCD_GREEN);
}

static float MeasMagDif;
//Display measured data
static void MeasurementModeDraw(DSP_RX rx)
{
    float VSWR = DSP_CalcVSWR(rx);
    float r = fabsf(crealf(rx));
    float im = cimagf(rx);
    float rp = 0.f, xp = 0.f;
    if (r > 0.05)
        rp = r + im * (im / r);
    else
        rp = 10000.0;
    if (im * im > 0.0025)
        xp = im + r * (r / im);
    else
        im = 10000.0;
    if (parallel == 1)
    {
        r = rp;
        im = xp;
    }
    char str[40] = "";
    MeasMagDif = DSP_MeasuredDiffdB();
    sprintf(str, "Magnitude diff %.2f dB     ", MeasMagDif);
    LCD_FillRect(LCD_MakePoint(1, 62), LCD_MakePoint(260, 151), BackGrColor);
    if (parallel == 1)
    {
        if (r >= 5000.0f)
            sprintf(str, "Rp > 5k ");
        else if (r >= 999.5f)
            sprintf(str, "Rp:%.1f k ", r / 1000.0f);
        else if (r >= 99.5f)
            sprintf(str, "Rp: %.1f ", r);
        else
            sprintf(str, "Rp: %.2f ", r);
        if (fabsf(im) >= 5000.0f)
            sprintf(&str[strlen(str)], "|Xp| > 5k");
        else if (fabsf(im) > 999.5f)
            sprintf(&str[strlen(str)], "Xp:%.1f k", im / 1000.0f);
        else if (fabsf(im) >= 199.5f)
            sprintf(&str[strlen(str)], "Xp: %.1f", im);
        else
            sprintf(&str[strlen(str)], "Xp: %.2f", im);
    }
    else
    {
        if (r >= 5000.0f)
            sprintf(str, "Rs > 5k ");
        else if (r >= 999.5f)
            sprintf(str, "Rs:%.1f k ", r / 1000.0f);
        else if (r >= 99.5f)
            sprintf(str, "Rs: %.1f ", r);
        else
            sprintf(str, "Rs: %.2f ", r);
        if (fabsf(im) >= 5000.0f)
            sprintf(&str[strlen(str)], "|Xs| > 5k");
        else if (fabsf(im) > 999.5f)
            sprintf(&str[strlen(str)], "Xs:%.1f k", im / 1000.0f);
        else if (fabsf(im) >= 199.5f)
            sprintf(&str[strlen(str)], "Xs: %.1f", im);
        else
            sprintf(&str[strlen(str)], "Xs: %.2f", im);
    }

    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 4, 92, str);

    //    FONT_ClearHalfLine(FONT_FRANBIG, BackGrColor, 62);
    if (VSWR > 100.0)
        sprintf(str, "VSWR: %.0f (Z0 %d)", VSWR, (int)CFG_GetParam(CFG_PARAM_R0));
    else
        sprintf(str, "VSWR: %.1f (Z0 %d)   ", VSWR, (int)CFG_GetParam(CFG_PARAM_R0));
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 4, 63, str);

    if (im >= 0)
    {
        //calculate equivalent capacity and inductance (if XX is big enough)
        if ((im > 1.0) && (im < 5000.0f)) // XX > 2.0 && XX < 1500.
        {
            float Luh = 1e6 * fabsf(im) / (2.0 * 3.1415926 * GEN_GetLastFreq());
            if (parallel == 1)
                sprintf(str, "Lp= %.2f uH   ", Luh);
            else
                sprintf(str, "Ls= %.2f uH   ", Luh);
        }
        else
        {
            if (parallel == 1)
                sprintf(str, "Lp out of range");
            else
                sprintf(str, "Ls out of range");
        }
    }
    else
    {
        if ((im < -1.0) && (im > -5000.0f))
        {
            float Cpf = 1e12 / (2.0 * 3.1415926 * GEN_GetLastFreq() * fabs(im));
            if (parallel == 1)
                sprintf(str, "Cp= %.0f pF    ", Cpf);
            else
                sprintf(str, "Cs= %.0f pF   ", Cpf);
        }
        else
        {
            if (parallel == 1)
                sprintf(str, "Cp out of range");
            else
                sprintf(str, "Cs out of range");
        }
    }
    FONT_Write(FONT_FRANBIG, Color1, BackGrColor, 4, 122, str);

    LCD_Rectangle(LCD_MakePoint(1, 65), LCD_MakePoint(274, 152), TextColor);
    //Calculated matched cable loss at this frequency
    FONT_ClearHalfLine(FONT_FRAN, BackGrColor, 158);
    float ga = cabsf(OSL_GFromZ(rx, CFG_GetParam(CFG_PARAM_R0))); //G amplitude
    if (ga > 0.01)
    {
        float cl = -10. * log10f(ga);
        if (cl < 0.001f)
            cl = 0.f;
        sprintf(str, "MCL: %.2f dB  |Z|: %.1f", cl, cabsf(rx));
        FONT_Write(FONT_FRAN, Color1, BackGrColor, 0, 158, str);
    }
}

//Draw a small (100x30 pixels) VSWR graph for data collected by Scan500()
static void MeasurementModeGraph(DSP_RX in)
{
    LCDPoint p1, p2;
    int idx = 0;
    float vswr;

    LCD_FillRect(LCD_MakePoint(SCAN_ORIGIN_X, SCAN_ORIGIN_Y - 9), LCD_MakePoint(SCAN_ORIGIN_X + 200, SCAN_ORIGIN_Y + 21), BackGrColor); // Graph rectangle

    //LCD_FillRect(LCD_MakePoint(SCAN_ORIGIN_X,   SCAN_ORIGIN_Y -  9), LCD_MakePoint(SCAN_ORIGIN_X + 200, SCAN_ORIGIN_Y + 21), BackGrColor); // Graph rectangle
    LCD_Line(LCD_MakePoint(SCAN_ORIGIN_X + 100, SCAN_ORIGIN_Y + 1), LCD_MakePoint(SCAN_ORIGIN_X + 100, SCAN_ORIGIN_Y + 11), TextColor); // Measurement frequency line
    LCD_Line(LCD_MakePoint(SCAN_ORIGIN_X + 50, SCAN_ORIGIN_Y + 1), LCD_MakePoint(SCAN_ORIGIN_X + 50, SCAN_ORIGIN_Y + 11), TextColor);
    LCD_Line(LCD_MakePoint(SCAN_ORIGIN_X + 150, SCAN_ORIGIN_Y + 1), LCD_MakePoint(SCAN_ORIGIN_X + 150, SCAN_ORIGIN_Y + 11), TextColor);
    LCD_Line(LCD_MakePoint(SCAN_ORIGIN_X, SCAN_ORIGIN_Y + 17), LCD_MakePoint(SCAN_ORIGIN_X + 200, SCAN_ORIGIN_Y + 17), TextColor); // VSWR 2.0 line
    for (idx = 0; idx < 100; idx++)
    {
        if (TOUCH_IsPressed())
            return;
        vswr = vswr500[idx];
        if (vswr != 9999.0)
        {
            if (vswr > 12.0 || isnan(vswr) || isinf(vswr) || vswr < 1.0) //Graph limit is VSWR 3.0
                vswr = 12.0;
            vswr = 10.2f * log10f(vswr); // smooth curve ** WK **                                               //Including uninitialized values

            if (idx == 0)
                p1 = LCD_MakePoint(SCAN_ORIGIN_X + (idx - 1) * 2, SCAN_ORIGIN_Y + 11 - (int)((vswr * 30 - 140) / 11));
            else
            {
                p2 = LCD_MakePoint(SCAN_ORIGIN_X + idx * 2, SCAN_ORIGIN_Y + 11 - (int)((vswr * 30 - 140) / 11));
                LCD_Line(p1, p2, CurvColor);
                LCD_Line((LCDPoint){p1.x, p1.y + 1}, (LCDPoint){p2.x, p2.y + 1}, CurvColor);
                p1 = p2;
            }
        }
        else if (idx == 0)
            p1 = LCD_MakePoint(SCAN_ORIGIN_X - 2, SCAN_ORIGIN_Y - 9);
    }
}

static void MEASUREMENT_Exit(void)
{
    MeasRqExit = 1;
}
static uint32_t fx = 14000000ul; //Scan range start frequency, in Hz

void FDecr(uint32_t step)
{
    uint32_t MeasurementFreq = CFG_GetParam(CFG_PARAM_MEAS_F);
    MeasurementFreq -= step;
    if (MeasurementFreq < CFG_GetParam(CFG_PARAM_BAND_FMIN))
        MeasurementFreq = CFG_GetParam(CFG_PARAM_BAND_FMIN);
    ;
    CFG_SetParam(CFG_PARAM_MEAS_F, MeasurementFreq);
    fChanged = 1;
    freqMHzf = MeasurementFreq / 1000000.;
    fx = MeasurementFreq;
    Sleep(50);
}

void FIncr(uint32_t step)
{
    uint32_t MeasurementFreq = CFG_GetParam(CFG_PARAM_MEAS_F);
    MeasurementFreq += step;
    if (MeasurementFreq > CFG_GetParam(CFG_PARAM_BAND_FMAX))
        MeasurementFreq = CFG_GetParam(CFG_PARAM_BAND_FMAX);
    ;
    CFG_SetParam(CFG_PARAM_MEAS_F, MeasurementFreq);
    fChanged = 1;
    freqMHzf = MeasurementFreq / 1000000.;
    fx = MeasurementFreq;
    Sleep(50);
}

static void MEASUREMENT_FDecr_500k(void)
{
    FDecr(meas_maxstep);
}
static void MEASUREMENT_FDecr_100k(void)
{
    FDecr(100000);
}
static void MEASUREMENT_FDecr_10k(void)
{
    FDecr(10000);
}
static void MEASUREMENT_FIncr_10k(void)
{
    FIncr(10000);
}
static void MEASUREMENT_FIncr_100k(void)
{
    FIncr(100000);
}
static void MEASUREMENT_FIncr_500k(void)
{
    FIncr(meas_maxstep);
}
static void MEASUREMENT_SmithMatch(void)
{
    isMatch = !isMatch;
    MeasRedrawWindow = 1;
    while (TOUCH_IsPressed())
        ; // WK
    Sleep(10);
}

static uint32_t fxkHz; //Scan range start frequency, in kHz
static BANDSPAN pBs1;

static void MEASUREMENT_SetFreq(void)
{
    //    while(TOUCH_IsPressed()); WK
    fx = CFG_GetParam(CFG_PARAM_MEAS_F);
    fxkHz = fx / 1000;
    if (PanFreqWindow(&fxkHz, &pBs1))
    {
        //Span or frequency has been changed
        CFG_SetParam(CFG_PARAM_MEAS_F, fxkHz * 1000);
    }
    // uint32_t val = NumKeypad(CFG_GetParam(CFG_PARAM_MEAS_F)/1000, BAND_FMIN/1000, CFG_GetParam(CFG_PARAM_BAND_FMAX)/1000, "Set measurement frequency, kHz");
    freqMHzf = fxkHz / 1000.;
    CFG_Flush();
    fChanged = 1;
    Sleep(200);
}

static void MEASUREMENT_Screenshot(void)
{
    char *fname = 0;
    fname = SCREENSHOT_SelectFileName();

    if (strlen(fname) == 0)
        return;

    SCREENSHOT_DeleteOldest();
    Date_Time_Stamp();
    if (CFG_GetParam(CFG_PARAM_SCREENSHOT_FORMAT))
        SCREENSHOT_SavePNG(fname);
    else
        SCREENSHOT_Save(fname);
}

static const struct HitRect MeasHitArr[] = // Set frequency...
    {
        //        x0,  y0, width, height, callback
        HITRECT(1, 236, 80, 35, MEASUREMENT_Exit),
        HITRECT(80, 236, 100, 35, MEASUREMENT_SetFreq),
        HITRECT(180, 236, 100, 35, MEASUREMENT_Screenshot),
        HITRECT(1, 1, 80, 35, MEASUREMENT_FDecr_500k),
        HITRECT(80, 1, 80, 35, MEASUREMENT_FDecr_100k),
        HITRECT(160, 1, 80, 35, MEASUREMENT_FDecr_10k),
        HITRECT(240, 1, 80, 35, MEASUREMENT_FIncr_10k),
        HITRECT(320, 1, 80, 35, MEASUREMENT_FIncr_100k),
        HITRECT(400, 1, 79, 35, MEASUREMENT_FIncr_500k),
        HITRECT(278, 90, 201, 181, MEASUREMENT_SmithMatch),
        HITRECT(278, 58, 100, 34, MEASUREMENT_SmithMatch),
        HITRECT(1, 63, 244, 89, MEASUREMENT_ParSerial),
        HITEND};

void DrawHeader(void)
{
    //Draw Top Menu
    LCD_DrawBitmap(LCD_MakePoint(0, 0), imgbtn_empty80, imgbtn_empty80_size);
    LCD_DrawBitmap(LCD_MakePoint(80, 0), imgbtn_empty80, imgbtn_empty80_size);
    LCD_DrawBitmap(LCD_MakePoint(160, 0), imgbtn_empty80, imgbtn_empty80_size);
    LCD_DrawBitmap(LCD_MakePoint(240, 0), imgbtn_empty80, imgbtn_empty80_size);
    LCD_DrawBitmap(LCD_MakePoint(320, 0), imgbtn_empty80, imgbtn_empty80_size);
    LCD_DrawBitmap(LCD_MakePoint(398, 0), imgbtn_empty80, imgbtn_empty80_size);
    LCD_Rectangle(LCD_MakePoint(0, 32), LCD_MakePoint(479, 33), LCD_RGB(200, 200, 200));
    LCD_Rectangle(LCD_MakePoint(478, 0), LCD_MakePoint(479, 33), LCD_RGB(200, 200, 200));

//#define FREQ_FONT_COLOR LCD_RGB(2, 16, 77)
#define FREQ_FONT_COLOR LCD_RGB(253, 229, 227)
    FONT_Write(FONT_FRAN, FREQ_FONT_COLOR, 1, 0 + 10, 10, "-500 kHz ");
    FONT_Write(FONT_FRAN, FREQ_FONT_COLOR, 1, 80 + 10, 10, "-100 kHz ");
    FONT_Write(FONT_FRAN, FREQ_FONT_COLOR, 1, 160 + 10, 10, " -10 kHz ");
    FONT_Write(FONT_FRAN, FREQ_FONT_COLOR, 1, 240 + 10, 10, " +10 kHz ");
    FONT_Write(FONT_FRAN, FREQ_FONT_COLOR, 1, 320 + 10, 10, "+100 kHz ");
    FONT_Write(FONT_FRAN, FREQ_FONT_COLOR, 1, 400 + 10, 10, "+500 kHz ");
}

//Measurement mode window. To change it to VSWR tap the lower part of display.
//To change frequency, in steps of +/- 500, 100 and 10 kHz, tap top part of the display,
//the step depends on how far you tap from the center.

void Single_Frequency_Proc(void)
{
    float delta, r, im;
    uint32_t fbkup, f_mess;

    int l, k;
    uint32_t speedcnt = 0;
    bool first = true;
    DSP_RX rx, rx0, rmid;
    LCDPoint pt;
    parallel = 0; // selects parallel/serial calculation
    DrawFine = 0;
    MeasRqExit = 0;
    MeasRedrawWindow = 0;
    fChanged = 1;
    isMatch = 0;
    SetColours();
    BSP_LCD_SelectLayer(0);
    LCD_FillAll(BackGrColor);
    BSP_LCD_SelectLayer(1);
    LCD_FillAll(BackGrColor);
    LCD_ShowActiveLayerOnly();

    //Load saved middle frequency value from BKUP registers 2, 3
    //to MeasurementFreq
    while (TOUCH_IsPressed())
        ;
    fbkup = CFG_GetParam(CFG_PARAM_MEAS_F);
    if (!(fbkup >= BAND_FMIN &&
          fbkup <= CFG_GetParam(CFG_PARAM_BAND_FMAX) &&
          (fbkup % 1000) == 0))
    {
        CFG_SetParam(CFG_PARAM_MEAS_F, 14000000ul);
        CFG_Flush();
    }

    LCD_FillRect(LCD_MakePoint(1, 1), LCD_MakePoint(479, 271), BackGrColor); // Graph rectangle
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 120, 60, "Single Frequency Mode");
    Sleep(1000);
    LCD_FillRect(LCD_MakePoint(120, 60), LCD_MakePoint(479, 140), BackGrColor); // Graph rectangle
    if (-1 == OSL_GetSelected())
    {
        FONT_Write(FONT_FRANBIG, LCD_RED, BackGrColor, 80, 120, "No calibration file selected!");
        Sleep(200);
    }

    //Changed Buttons Layout to Bitmap style for Users request
    //KD8CEC
    //Draw Bottom Menu (Main Menu)
    LCD_DrawBitmap(LCD_MakePoint(0, 236), imgbtn_home2, imgbtn_home2_size);
    LCD_DrawBitmap(LCD_MakePoint(78, 236), imgbtn_empty2, imgbtn_empty2_size);
    FONT_Write(FONT_FRAN, TextColor, 0, 80 + 10, 235 + 10, "Set Frequency");
    LCD_DrawBitmap(LCD_MakePoint(176, 236), imgbtn_empty2, imgbtn_empty2_size);
    FONT_Write(FONT_FRAN, TextColor, 0, 180 + 10, 235 + 10, "Save Snapshot");
    LCD_Rectangle(LCD_MakePoint(0, 269), LCD_MakePoint(275, 270), LCD_RGB(200, 200, 200));
    LCD_Rectangle(LCD_MakePoint(274, 237), LCD_MakePoint(275, 270), LCD_RGB(200, 200, 200));

    DrawHeader();
    //Draw Smitchchart area
    LCD_Rectangle(LCD_MakePoint(277, 90), LCD_MakePoint(479, 271), LCD_RGB(150, 150, 150));
    LCD_Rectangle(LCD_MakePoint(277, 65), LCD_MakePoint(277 + 99 - 1, 58 + 30 - 1), LCD_RGB(150, 150, 150));

    freqOld = 0; //to ensure that frequency is shown in next step
    ShowF();

    //   LCD_Rectangle(LCD_MakePoint(SCAN_ORIGIN_X - 1, SCAN_ORIGIN_Y-10), LCD_MakePoint(SCAN_ORIGIN_X + 201, SCAN_ORIGIN_Y + 22), LCD_BLUE);
    InitScan500();
    //ShowHitRect(MeasHitArr);
    //ShowIncDec();

    //MEASUREMENT_REDRAW:
    for (;;)
    {
        while (TOUCH_Poll(&pt))
        {
            if (HitTest(MeasHitArr, pt.x, pt.y) == 1)
            { //any button pressed?
                if (fChanged)
                {
                    MeasRedrawWindow = 1;
                    ShowF();
                }
                if (MeasRqExit + MeasRedrawWindow)
                    break;
            }
            speedcnt++;
            if (speedcnt < 20)
                Sleep(100);
            else
                Sleep(30);
            if (speedcnt > 50)
                meas_maxstep = 2000000;
        }

        MeasRedrawWindow = 0;
        LCD_FillRect(LCD_MakePoint(SCAN_ORIGIN_X, SCAN_ORIGIN_Y - 9), LCD_MakePoint(SCAN_ORIGIN_X + 200, SCAN_ORIGIN_Y + 21), BackGrColor); // Graph rectangle
        LCD_Rectangle(LCD_MakePoint(SCAN_ORIGIN_X - 5, SCAN_ORIGIN_Y - 10), LCD_MakePoint(SCAN_ORIGIN_X + 201, SCAN_ORIGIN_Y + 22), LCD_BLUE);
        FONT_Write(FONT_FRAN, TextColor, BackGrColor, SCAN_ORIGIN_X - 20, SCAN_ORIGIN_Y - 35, "VSWR (1.0 ... 12.0), F +/- 500 KHz, step 10:");
        ShowF();
        if (-1 == OSL_GetSelected())
        {
            FONT_Write(FONT_FRAN, LCD_RED, BackGrColor, 385, 54, "No OSL"); // WK
        }
        else
        {
            OSL_CorrectZ(BAND_FMIN, 0 + 0 * I); //To force lazy loading OSL file if it has not been loaded yet
            if (OSL_IsSelectedValid())
            {
                FONT_Print(FONT_FRAN, Color1, BackGrColor, 385, 54, "OSL: %s, OK", OSL_GetSelectedName()); // WK
            }
            else
            {
                FONT_Print(FONT_FRAN, LCD_YELLOW, BackGrColor, 385, 54, "OSL: %s, BAD", OSL_GetSelectedName()); // WK
            }
        }

        if (OSL_IsErrCorrLoaded())
        {
            FONT_Write(FONT_FRAN, Color1, BackGrColor, 385, 70, "HW cal: OK"); // WK
        }
        else
        {
            FONT_Write(FONT_FRAN, LCD_RED, BackGrColor, 385, 70, "HW cal: NO"); // WK
        }
        f_mess = CFG_GetParam(CFG_PARAM_MEAS_F);
        GEN_SetMeasurementFreq(f_mess);
        DSP_Measure(f_mess, 1, 1, 1);
        Sleep(2);
        rmid = rx = DSP_MeasuredZ();

        DrawSmallSmith(380, 180, 80, rx);
        MeasurementModeDraw(rx);

        if (MeasMagDif >= 10.5f)
        {
            Sleep(1000);
        }
        speedcnt = 0; // ********************** ??
                      //Main window cycle
        l = k = 0;
        while (!MeasRedrawWindow)
        {
            while (TOUCH_Poll(&pt))
            {
                if (HitTest(MeasHitArr, pt.x, pt.y) == 1)
                { //any button pressed?
                    if (fChanged)
                    {
                        MeasRedrawWindow = 1;
                        ShowF();
                    }
                    if (MeasRqExit + MeasRedrawWindow)
                        break;
                }
                speedcnt++;
                if (speedcnt < 20)
                    Sleep(100);
                else
                    Sleep(30);
                if (speedcnt > 50)
                    meas_maxstep = 2000000;
            }
            Scan500(l++, k);

            f_mess = CFG_GetParam(CFG_PARAM_MEAS_F);
            GEN_SetMeasurementFreq(f_mess);
            Sleep(2);
            DSP_Measure(f_mess, 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
            rx0 = DSP_MeasuredZ();
            if (first || (l == 1))
            {
                first = false;
                rmid = rx0;
            }
            else
            {
                r = crealf(rx0);
                im = cimagf(rx0);
                rmid = 0.95 * crealf(rmid) + 0.05f * r + I * (0.95f * cimagf(rmid) + 0.05f * im);
            }
            if (l >= 20)
            {
                l = 0;
                k = (k + 2) % 5; //0 2 4 1 3 0...

                MeasurementModeDraw(rmid);
                if (MeasMagDif < 10.5f)
                {
                    DrawSmallSmith(380, 180, 80, rx);
                    MeasurementModeGraph(rx);
                }
                else
                    Sleep(10); // open - nothing connected
            }

            // DSP_Measure(CFG_GetParam(CFG_PARAM_MEAS_F), 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
            // rx0 = DSP_MeasuredZ();
            delta = fabsf(crealf(rx0) - crealf(rx));
            delta += fabsf(cimagf(rx0) - cimagf(rx));
            delta = delta / (crealf(rx0) + crealf(rx));
            if (delta > 0.4)
            { // was 0.06
                MeasRedrawWindow = 1;
                InitScan500();
                rx = rx0;
            }
            //if (DSP_MeasuredMagVmv() < 1.)
            if (DSP_MeasuredMagVmv() < 0.3f)
            {
                FONT_Write(FONT_FRAN, LCD_RED, BackGrColor, 385, 38, "No signal  "); // WK
            }
            else
            {
                FONT_Write(FONT_FRAN, Color1, BackGrColor, 385, 38, "Signal OK  "); // WK
            }

            if (!isMatch)
                FONT_Write(FONT_FRAN, TextColor, BackGrColor, 295, 68, "LC match      ");
            else
            {
                FONT_Write(FONT_FRAN, TextColor, BackGrColor, 295, 68, "Smith diagram ");
                //MeasureFrequency();
            }
            uint32_t speedcnt = 0;
            meas_maxstep = 500000;

            while (TOUCH_Poll(&pt))
            {
                if (HitTest(MeasHitArr, pt.x, pt.y) == 1)
                { //any button pressed?
                    if (fChanged)
                    {
                        MeasRedrawWindow = 1;
                        ShowF();
                    }
                    if (MeasRqExit + MeasRedrawWindow)
                        break;
                }
                speedcnt++;
                if (speedcnt < 20)
                    Sleep(100);
                else
                    Sleep(30);
                if (speedcnt > 50)
                    meas_maxstep = 2000000;
            }

            if (MeasRqExit)
            {
                GEN_SetMeasurementFreq(0);
                TRACK_Beep(1);

                return;
            }
            speedcnt = 0;
            meas_maxstep = 500000;
            if (fChanged)
            {
                MeasRedrawWindow = 1;
                InitScan500();
                CFG_Flush();
                fChanged = 0;
            }
            Sleep(5);
        }
    }
}

void setup_GPIO(void) // GPIO I Pin 2 for buzzer
{
    GPIO_InitTypeDef gpioInitStructure;

    __HAL_RCC_GPIOI_CLK_ENABLE();
    gpioInitStructure.Pin = GPIO_PIN_2;
    gpioInitStructure.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Pull = GPIO_NOPULL;
    gpioInitStructure.Speed = GPIO_SPEED_MEDIUM;
    HAL_GPIO_Init(GPIOI, &gpioInitStructure);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_2, 0);
    //AUDIO1=0;
}

static int muted;
static uint32_t ToneFreq, last_TuneFreq;
static int ToneTrigger;
static TEXTBOX_CTX_t SWR1;
int Tone;
uint8_t SWRLimit;
int rqExitSWR3;
extern float freqMHzf;

//extern void ShowFr(int);

#define M_BGCOLOR LCD_RGB(0, 0, 64)    //Menu item background color
#define M_FGCOLOR LCD_RGB(255, 255, 0) //Menu item foreground color

void DrawTuneButtonText(void)
{
    FONT_Write(FONT_FRAN, TextColor, 0, 180 + 20, 237 + 10, "Gen Tone");
    LCD_FillRect(LCD_MakePoint(180 + 5, 237 + 8),
                 LCD_MakePoint(180 + 15, 237 + 27), Tone ? LCD_BLUE : LCD_RGB(50, 50, 50));

    FONT_Write(FONT_FRAN, TextColor, 0, 280 + 20, 237 + 10, "SWR > 2.0");
    LCD_FillRect(LCD_MakePoint(280 + 5, 237 + 8),
                 LCD_MakePoint(280 + 15, 237 + 27), SWRLimit == 2 ? LCD_RED : LCD_RGB(50, 50, 50));

    FONT_Write(FONT_FRAN, TextColor, 0, 380 + 20, 237 + 10, "SWR > 3.0");
    LCD_FillRect(LCD_MakePoint(380 + 5, 237 + 8),
                 LCD_MakePoint(380 + 15, 237 + 27), SWRLimit == 3 ? LCD_RED : LCD_RGB(50, 50, 50));
}

void SWR_Mute(void)
{
    if (muted == 1)
    {
        muted = 0;
        SWRTone = 1; //tone
        FONT_Write(FONT_FRANBIG, M_FGCOLOR, M_BGCOLOR, 198, 226, " Mute ");
        SWRLimit = 1;
        UB_TIMER2_Init_FRQ(ToneFreq); //100...1000 Hz
        UB_TIMER2_Start();
    }
    else
    {
        muted = 1;
        SWRTone = 0;
        FONT_Write(FONT_FRANBIG, M_FGCOLOR, M_BGCOLOR, 198, 226, " Tone ");
        UB_TIMER2_Stop();
    }
    while (TOUCH_IsPressed())
        ;
    Sleep(50);
}

static void SWR_2(void)
{
    if (SWRLimit == 3)
        LCD_Rectangle((LCDPoint){380, 220}, (LCDPoint){476, 265}, 0xffffff00); //yellow
    if (SWRLimit == 2)
    {
        SWRLimit = 1;
        LCD_Rectangle((LCDPoint){280, 220}, (LCDPoint){380, 265}, 0xffffff00);
    }
    else
    {
        SWRLimit = 2;
        LCD_Rectangle((LCDPoint){280, 220}, (LCDPoint){380, 265}, 0xffff0000);
    }
    while (TOUCH_IsPressed())
        ;
    Sleep(50);
    SWRTone = 1;
    muted = 0;
    ToneTrigger = 1;
}

static void SWR_3(void)
{
    if (SWRLimit == 2)
        LCD_Rectangle((LCDPoint){280, 220}, (LCDPoint){380, 265}, 0xffffff00);
    if (SWRLimit == 3)
    {
        SWRLimit = 1;
        LCD_Rectangle((LCDPoint){380, 220}, (LCDPoint){476, 265}, 0xffffff00);
    }
    else
    {
        SWRLimit = 3;
        LCD_Rectangle((LCDPoint){380, 220}, (LCDPoint){476, 265}, 0xffff0000); // red
    }
    while (TOUCH_IsPressed())
        ;
    Sleep(50);
    SWRTone = 1;
    muted = 0;
    ToneTrigger = 1;
}

void SWR_SetFrequencyMuted(void)
{
    if (Tone == 1)
    {
        Tone = 0;
        UB_TIMER2_Stop();
    }
    else
    {
        Tone = 1;
        SWR_SetFrequency();
        UB_TIMER2_Init_FRQ(last_TuneFreq);
        UB_TIMER2_Start();
    }
}

static const struct HitRect TuneSWR_HitArr[] = // Set frequency...
    {
        //        x0,  y0, width, height, callback
        HITRECT(0, 225, 70, 45, MEASUREMENT_Exit),
        HITRECT(70, 225, 120, 45, MEASUREMENT_SetFreq),
        HITRECT(190, 225, 90, 45, SWR_Mute),
        HITRECT(280, 225, 100, 45, SWR_2),
        HITRECT(380, 225, 96, 45, SWR_3),
        HITRECT(1, 1, 80, 35, MEASUREMENT_FDecr_500k),
        HITRECT(80, 1, 80, 35, MEASUREMENT_FDecr_100k),
        HITRECT(160, 1, 80, 35, MEASUREMENT_FDecr_10k),
        HITRECT(240, 1, 80, 35, MEASUREMENT_FIncr_10k),
        HITRECT(320, 1, 80, 35, MEASUREMENT_FIncr_100k),
        HITRECT(400, 1, 79, 35, MEASUREMENT_FIncr_500k),
        HITEND};

static const TEXTBOX_t tb_menuSWR[] = {
    (TEXTBOX_t){.x0 = 70, .y0 = 220, .text = "Frequency", .font = FONT_FRANBIG, .width = 120, .height = 45, .center = 1, .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cbparam = 1, .next = (void *)&tb_menuSWR[1]},
    (TEXTBOX_t){.x0 = 280, .y0 = 220, .text = "SWR_2", .font = FONT_FRANBIG, .width = 100, .height = 45, .center = 1, .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cbparam = 1, .next = (void *)&tb_menuSWR[2]},
    (TEXTBOX_t){.x0 = 380, .y0 = 220, .text = "SWR_3", .font = FONT_FRANBIG, .width = 96, .height = 45, .center = 1, .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cbparam = 1, .next = (void *)&tb_menuSWR[3]},
    (TEXTBOX_t){.x0 = 190, .y0 = 220, .text = "Mute", .font = FONT_FRANBIG, .width = 90, .height = 45, .center = 1, .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cbparam = 1, .next = (void *)&tb_menuSWR[4]},
    (TEXTBOX_t){
        .x0 = 0,
        .y0 = 220,
        .text = "Exit",
        .font = FONT_FRANBIG,
        .width = 70,
        .height = 45,
        .center = 1,
        .border = 1,
        .fgcolor = M_FGCOLOR,
        .bgcolor = LCD_RED,
        .cbparam = 1,
    },
};

void Tune_SWR_Proc(void)
{ // ==========================================================================

    char str[20];
    float vswrf = 1.f, vswrf_old, vswLogf, SwrDiff;
    uint32_t width, SWRToneOld = 0;
    uint32_t color1, k = 0;
    static LCDPoint pt;

    meas_maxstep = 500000;
    freqMHzf = CFG_GetParam(CFG_PARAM_MEAS_F) / 1000000.;
    SWRLimit = 1;
    ToneTrigger = 0;
    setup_GPIO(); // GPIO I Pin 2 for buzzer
    MeasRqExit = 0;
    SetColours();
    LCD_FillAll(BackGrColor);
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 120, 40, "Tune SWR ");
    Sleep(1000);
    while (TOUCH_IsPressed())
        ;
    ShowFr(2); //reads frequency from file
    TEXTBOX_InitContext(&SWR1);
    muted = 0;   // begin without mute
    SWRTone = 1; //tone on
    SWRToneOld = 0;
    vswrf_old = 0;
    TEXTBOX_Append(&SWR1, (TEXTBOX_t *)tb_menuSWR);
    TEXTBOX_DrawContext(&SWR1);
    DrawHeader();
    ToneFreq = 440;
    UB_TIMER2_Init_FRQ(ToneFreq); //100...1000 Hz
    UB_TIMER2_Start();

    for (;;)
    {
        Sleep(0); //for autosleep to work
        if ((SWRToneOld != SWRTone) || (ToneTrigger == 1))
        {
            SWRToneOld = SWRTone;
            ToneTrigger = 0;
            if (SWRTone == 0)
                UB_TIMER2_Stop();
        }
        if (TOUCH_Poll(&pt))
        {
            if (HitTest(TuneSWR_HitArr, pt.x, pt.y) == 1)
            { //any button pressed?

                if (fChanged)
                {
                    fChanged = 0;
                    ShowFr(2);
                    CFG_Flush();
                }
                if (MeasRqExit == 1)
                {
                    SWRTone = 0;
                    MeasRqExit = 0;
                    GEN_SetMeasurementFreq(0);
                    UB_TIMER2_Stop();
                    return;
                }
            }
        }
        k++;
        if (k >= 5)
        {
            k = 0;
            DSP_Measure(freqMHzf * 1000000., 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
            vswrf = DSP_CalcVSWR(DSP_MeasuredZ());
            SwrDiff = vswrf_old - vswrf;
            if (SwrDiff < 0)
                SwrDiff = -SwrDiff;
            if (SwrDiff > 0.01 * vswrf)
            { // Difference more than 1 %
                ToneTrigger = 1;
                vswrf_old = vswrf;
                if (muted == 0)
                {
                    SWRTone = 1;
                    if (SWRLimit == 2)
                    {
                        if (vswrf > 2.0)
                        {
                            SWRTone = 0;
                        }
                    }
                    else if (SWRLimit == 3)
                    {
                        if (vswrf > 3.0)
                        {
                            SWRTone = 0;
                        }
                    }
                }
                sprintf(str, "SWR: %.1f  ", vswrf);
                LCD_FillRect(LCD_MakePoint(200, 70), LCD_MakePoint(470, 125), BackGrColor);
                FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 200, 70, str);
                if ((SWRTone == 1) && (muted == 0))
                {
                    UB_TIMER2_Init_FRQ(ToneFreq); //100...1000 Hz
                    UB_TIMER2_Start();
                }
                else
                    UB_TIMER2_Stop();
            }
            Sleep(5);
        }
        vswLogf = 200.0 * log10f(10.0 * log10f(vswrf) + 5.0);
        ToneFreq = (uint32_t)(6.0 * vswLogf - 250.0);
        width = (uint32_t)(3 * vswLogf - 400.0);
        if (width > 479)
            width = 479;
        if (vswrf < 2.0)
            color1 = 0xff00ff00;
        else if (vswrf < 3)
            color1 = 0xffffff00;
        else
            color1 = 0xffff0000;
        LCD_FillRect(LCD_MakePoint(0, 116), LCD_MakePoint(width, 160), TextColor);
        LCD_FillRect(LCD_MakePoint(0, 161), LCD_MakePoint(width, 205), color1);
        LCD_FillRect(LCD_MakePoint(width + 1, 116), LCD_MakePoint(479, 205), BackGrColor);
        Sleep(10); //5
    }
}
