/*
  S21-Gain for EU1KY's AA

  by KD8CEC
  kd8cec@gmail.com
  -----------------------------------------------------------------------------
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>

#include "LCD.h"
#include "touch.h"
#include "font.h"
#include "config.h"
#include "ff.h"
#include "crash.h"
#include "dsp.h"
#include "gen.h"
#include "oslfile.h"
#include "spectr.h"
#include "stm32746g_discovery_lcd.h"
#include "screenshot.h"
#include "panvswr2.h"
#include "panfreq.h"
#include "smith.h"
#include "textbox.h"
#include "generator.h"
#include "FreqCounter.h"
#include "bitmaps/bitmaps.h"
#include "si5351.h"
#include "si5351_hs.h"
#include "sdram_heap.h"

#define X0 51
#define Y0 18
#define WWIDTH 400
#define WHEIGHT 190
#define WY(offset) ((WHEIGHT + Y0) - (offset))
#define WGRIDCOLOR LCD_COLOR_DARKGRAY
#define RED1 LCD_RGB(245, 0, 0)
#define RED2 LCD_RGB(235, 0, 0)

extern uint8_t AUDIO1;

static void save_snapshot(void);

static int redrawRequired, exitScan;

//static const char *modstr = "CEC v." AAVERSION_CECV " ";

//To have the same range to reduce memory usage
extern const char *BSSTR[];
extern const char *BSSTR_HALF[];
extern const uint32_t BSVALUES[];

#define BSSTR_TRACK BSSTR
#define BSSTR_TRACK_HALF BSSTR_HALF
#define BSVALUES_TRACK BSVALUES

static char autoScanFactor = 1;
static uint32_t f1 = 14000000; //Scan range start frequency, in Hz
static BANDSPAN span21 = BS400;
static float fcur; // frequency at cursor position in kHz
//static char buf[64];
static LCDPoint pt0;
float *valuesmI;
float *valuesdBI;

#define displayRate 2.9f //Display Zoom
#define displayOffset 190
//static char DisplayType = 0;      //values or valuesmI
static int isMeasured = 0;
static uint32_t cursorPos = WWIDTH / 2;

static uint32_t cursorChangeCount = 0;
static uint32_t autofast = 0;
//static void Track_DrawRX();
static int trackbeep;
static char tmpBuff[50];

static uint32_t activeLayerS21;
static int cursorVisibleS21;
//static int firstRun;
static int FirstRunS21;

//=====================================================================
//Menu
//---------------------------------------------------------------------
#define S21_BOTTOM_MENU_TOP 243
#define trackMenu_Length 11
static const int trackMenus[trackMenu_Length][4] = {
    {0, S21_BOTTOM_MENU_TOP, 79, 271},    //HOME BUTTON (0)
    {81, S21_BOTTOM_MENU_TOP, 164, 271},  //SCAN (1)
    {165, S21_BOTTOM_MENU_TOP, 216, 271}, //ZOOM OUT (2)
    {217, S21_BOTTOM_MENU_TOP, 268, 271}, //ZOOM IN (3)
    {269, S21_BOTTOM_MENU_TOP, 352, 271}, //FREQ (4)
    {40, 0, 479, 80},                     //FREQ (5)
    {353, S21_BOTTOM_MENU_TOP, 435, 271}, //CAPTURE (6)
    {40, 210, 450, 235},                  //SetCursor DH1AKF (7)
    {430, 240, 479, 271},                 //Auto (8)
    {0, 90, 50, 170},                     //    < (9)
    {430, 90, 479, 170},                  //   > (10)
};

void TRACK_Beep(int duration)
{
    if (BeepOn1 == 0)
        return;

    if (trackbeep == 0)
    {
        trackbeep = 1;
        AUDIO1 = 1;
        UB_TIMER2_Init_FRQ(880);
        UB_TIMER2_Start();
        Sleep(100);
        AUDIO1 = 0;
        // UB_TIMER2_Stop();
    }

    if (duration == 1)
        trackbeep = 0;
}

static void DrawAutoText(void)
{
    static const char *atxt = "  Auto  ";
    if (0 == autofast)
        FONT_Print(FONT_FRAN, TextColor, BackGrColor, 435, trackMenus[8][BUTTON_TOP] + 10, atxt);
    else
        FONT_Print(FONT_FRAN, TextColor, LCD_MakeRGB(0, 128, 0), 435, trackMenus[8][BUTTON_TOP] + 10, atxt); //green background
}

//imgbtn_home
static void TRACK_DrawFootText(void)
{
    LCD_DrawBitmap(LCD_MakePoint(trackMenus[0][BUTTON_LEFT], trackMenus[0][BUTTON_TOP]), imgbtn_home1, imgbtn_home1_size);
    LCD_DrawBitmap(LCD_MakePoint(trackMenus[1][BUTTON_LEFT], trackMenus[1][BUTTON_TOP]), imgbtn_scan, imgbtn_scan_size);
    LCD_DrawBitmap(LCD_MakePoint(trackMenus[2][BUTTON_LEFT], trackMenus[2][BUTTON_TOP]), imgbtn_zoomout, imgbtn_zoomout_size);
    LCD_DrawBitmap(LCD_MakePoint(trackMenus[3][BUTTON_LEFT], trackMenus[3][BUTTON_TOP]), imgbtn_zoomin, imgbtn_zoomin_size);
    LCD_DrawBitmap(LCD_MakePoint(trackMenus[4][BUTTON_LEFT], trackMenus[4][BUTTON_TOP]), imgbtn_freq, imgbtn_freq_size);
    LCD_DrawBitmap(LCD_MakePoint(trackMenus[6][BUTTON_LEFT], trackMenus[6][BUTTON_TOP]), imgbtn_capture, imgbtn_capture_size);

    DrawAutoText();
    //Sleep(100);
}

float RawVoltage2;

static void DrawMeasuredValues(void)
{

    float t1 = -valuesmI[cursorPos]; // output 5 dBm

    float voltage1 = powf(10.f, (t1 + 5.f) / 20.f) / sqrtf(50); //U = sqrt(P/R)
    //int dB = CFG_GetParam(CFG_PARAM_ATTENUATOR);
    uint32_t fstart;  // in Hz
    uint32_t fCursor; // in Hz

    if (CFG_GetParam(CFG_PARAM_PAN_CENTER_F) == 0)
        fstart = f1;
    else
        fstart = f1 - 500 * BSVALUES_TRACK[span21];

    fCursor = fstart + (uint32_t)((cursorPos * BSVALUES_TRACK[span21] * 1000.f) / WWIDTH);

    if (fCursor > CFG_GetParam(CFG_PARAM_BAND_FMAX))
        fCursor = CFG_GetParam(CFG_PARAM_BAND_FMAX);
    LCD_FillRect(LCD_MakePoint(0, 228), LCD_MakePoint(479, 240), BackGrColor);

    FONT_Print(FONT_FRAN, TextColor, BackGrColor, 0, 225, "F: %8.2f MHz   dB: %4.1f   V:%7.3f mV",
               (float)(fCursor / 1000000.f), t1, voltage1 * 1000.f);

    //
    /* FONT_Print(FONT_FRAN, TextColor, BackGrColor, 240, 225, "t1: %g t2: %g",
               value1, value2); */
}

bool IsInverted;

static void DrawCursorS21(int offset)
{
    int i;
    int x = X0 + cursorPos + offset;
    if ((x < X0) || (x > X0 + WWIDTH))
        return;
    LCDPoint p;
    //Draw cursor line in TextColor
    p = LCD_MakePoint(x, Y0);

    i = 0;
    for (p.y = Y0; p.y < Y0 + WHEIGHT; p.y++)
    {
        if (i < 10)
            LCD_SetPixel(p, TextColor); //was: WK_InvertPixel(p.x,p.y);
        i++;
        if (i >= 20)
            i = 0;
    }
    if (offset == 0)
    {
        LCD_FillRect((LCDPoint){X0 + cursorPos - 3, Y0 + WHEIGHT + 2}, (LCDPoint){X0 + cursorPos + 3, Y0 + WHEIGHT + 4}, BackGrColor);
        LCD_FillRect((LCDPoint){X0 + cursorPos - 2, Y0 + WHEIGHT + 2}, (LCDPoint){X0 + cursorPos + 2, Y0 + WHEIGHT + 4}, TextColor);
    }
    Sleep(1);
}

static LCDColor c, c31, c35;
static int r, k, x, pos;

static void StrictDelCursor1(int offset)
{
    x = X0 + cursorPos + offset;
    if ((x < X0) || (x > X0 + WWIDTH))
        return;
    c31 = LCD_ReadPixel(LCD_MakePoint(x, Y0 + 31)); // no horizontal line between 30..39
    c35 = LCD_ReadPixel(LCD_MakePoint(x, Y0 + 35));
    c = c31;
    if (c31 == CurvColor)
        c = c35;
    LCD_VLine(LCD_MakePoint(x, Y0), WHEIGHT + 2, c); //draw original
    // draw horiz. lines:
    r = 10 * Y0;
    for (k = 0; k < 14; k++)
    { // horizontal lines
        LCD_SetPixel(LCD_MakePoint(x, r / 10), WGRIDCOLOR);
        if (k % 2 == 0)
            LCD_SetPixel(LCD_MakePoint(x, r / 10 + 1), WGRIDCOLOR);
        r += 146;
    }
    pos = Y0 + valuesmI[cursorPos + offset] / 65.f * WHEIGHT;
    LCD_SetPixel(LCD_MakePoint(x, pos), CurvColor); // set the new pixel(s)
    if (FatLines)
    {
        LCD_SetPixel(LCD_MakePoint(x, pos - 1), CurvColor); // WK
        LCD_SetPixel(LCD_MakePoint(x, pos + 1), CurvColor); // WK
    }
}

static void StrictDelCursor(void)
{
    StrictDelCursor1(0);
    if (FatLines)
    {
        StrictDelCursor1(-1);
        StrictDelCursor1(1);
    }
}

static void StrictDrawCursor(void)
{
    DrawCursorS21(0);
    if (FatLines)
    {
        DrawCursorS21(-1);
        DrawCursorS21(1);
    }
}

static void MoveCursor(int moveDirection)
{
    int NewPosition = cursorPos + moveDirection;

    if (!isMeasured)
        return;

    if ((NewPosition > WWIDTH) || (NewPosition < 0))
        return;

    StrictDelCursor(); // delete the old cursor
    cursorPos = NewPosition;
    StrictDrawCursor(); // set the new cursor

    if (cursorChangeCount++ < 5)
    {
        Sleep(20); //Slow down at first steps
    }
    DrawMeasuredValues(); //DH1AKF  25.10.2020
    Sleep(2);
}

#define linediv 10

static void Track_DrawGrid(int justGraphDraw) //
{
    char buf[30];
    int i, r;
    int verticalPos = 125;
    uint32_t fstart;
    //uint32_t pos = 130;

#define VerticalStep 146
#define VerticalX 10

    if (0 == CFG_GetParam(CFG_PARAM_PAN_CENTER_F))
    {
        fstart = f1;
        sprintf(buf, " graph: %.3f MHz +%s", (float)f1 / 1000000, BSSTR_TRACK[span21]);
    }

    else
    {
        fstart = f1 - 500 * BSVALUES_TRACK[span21];
        sprintf(buf, " graph: %.3f MHz +/- %s", (float)f1 / 1000000, BSSTR_TRACK_HALF[span21]);
    }
    LCD_FillRect(LCD_MakePoint(40, 15), LCD_MakePoint(459, 225), BackGrColor);
    cursorVisibleS21 = 0;
    if (justGraphDraw % 1 == 0)
    {
        //Vertical Numbers
        FONT_Write(FONT_FRAN, CurvColor, BackGrColor, VerticalX, Y0, " 0dB");
        r = 10 * Y0;
        verticalPos += VerticalStep;
        for (i = 10; i <= 75; i += 5)
        {
            verticalPos += VerticalStep;
            LCD_HLine(LCD_MakePoint(X0 - 5, r / 10), WWIDTH + 5, WGRIDCOLOR);
            if (i % 10 == 0)
            {
                LCD_HLine(LCD_MakePoint(X0 - 5, r / 10 + 1), WWIDTH + 5, WGRIDCOLOR);
                if (i <= 60)
                    FONT_Print(FONT_FRAN, CurvColor, BackGrColor, VerticalX, verticalPos / 10, "%d", -i); // - xx
            }
            r += VerticalStep;
        }

        //FONT_Write(FONT_FRAN, LCD_BLACK, LCD_PURPLE, 50, 0, modstr);
        FONT_Write(FONT_FRANBIG, TextColor, 0, 2, 107, "<");
        FONT_Write(FONT_FRANBIG, TextColor, 0, 460, 107, ">");

        FONT_Write(FONT_FRAN, CurvColor, BackGrColor, 230, 0, "|S21|Gain");
        FONT_Write(FONT_FRAN, TextColor, BackGrColor, 280, 0, buf); //LCD_BLUE
    }

    //Draw F grid and labels
    int lmod = 5;
    //Draw vertical line every linediv pixels

    //Mark ham bands with colored background DH1AKF
    for (i = 0; i <= WWIDTH; i++)
    {
        uint32_t f = fstart / 1000 + (i * BSVALUES[span21]) / WWIDTH;
        if (IsFinHamBands(f))
        {
            LCD_VLine(LCD_MakePoint(X0 + i, Y0), WHEIGHT + 1, LCD_COLOR_BLUE); // (0, 0, 64) darkblue << >> yellow
        }
    }

    for (i = 0; i <= WWIDTH / linediv; i++)
    {
        int x = X0 + i * linediv;
        if ((i % lmod) == 0 || i == WWIDTH / linediv)
        {
            //char fr[10];
            float flabel = ((float)(fstart / 1000. + i * BSVALUES_TRACK[span21] / (WWIDTH / linediv))) / 1000.f;
            if (flabel * 1000000.f > (float)(CFG_GetParam(CFG_PARAM_BAND_FMAX) + 1))
                continue;
            if (flabel > 999.99)
                sprintf(buf, "%.1f", ((float)(fstart / 1000. + i * BSVALUES_TRACK[span21] / (WWIDTH / linediv))) / 1000.f);
            else if (flabel > 99.99)
                sprintf(buf, "%.2f", ((float)(fstart / 1000. + i * BSVALUES_TRACK[span21] / (WWIDTH / linediv))) / 1000.f);
            else
                sprintf(buf, "%.3f", ((float)(fstart / 1000. + i * BSVALUES_TRACK[span21] / (WWIDTH / linediv))) / 1000.f); // WK
            int w = FONT_GetStrPixelWidth(FONT_SDIGITS, buf);
            // FONT_Write(FONT_SDIGITS, LCD_WHITE, LCD_BLACK, x - w / 2, Y0 + WHEIGHT + 5, f);// WK
            FONT_Write(FONT_FRAN, TextColor, BackGrColor, x - 8 - w / 2, Y0 + WHEIGHT + 3, buf);
            LCD_VLine(LCD_MakePoint(x, Y0), WHEIGHT + 1, WGRIDCOLOR);
            LCD_VLine(LCD_MakePoint(x + 1, Y0), WHEIGHT + 1, WGRIDCOLOR); // WK
        }
        else
        {
            LCD_VLine(LCD_MakePoint(x, Y0), WHEIGHT + 1, WGRIDCOLOR);
        }
    }
    if (justGraphDraw == 2)
    {
        TRACK_DrawFootText();
    }
    //LCD_FillRect((LCDPoint){X0 ,Y0+WHEIGHT+1},(LCDPoint){X0 + WWIDTH+2,Y0+WHEIGHT+3},BackGrColor);
}

//static uint32_t Fs, Fp;// in Hz
static float Cp; //, Rs;

static void Scan21(int selector)
{
    float inputMI;
    uint32_t i, nScanCount;
    uint32_t fstart, freq1, deltaF;

    f1 = CFG_GetParam(CFG_PARAM_S21_F1);
    if (CFG_GetParam(CFG_PARAM_PAN_CENTER_F) == 0)
        fstart = f1;
    else
        fstart = f1 - 500 * BSVALUES_TRACK[span21]; // 2;

    freq1 = fstart - 150000;
    if (freq1 < 100000)
        freq1 = 100000;
    CLK2_drive = 0;
    HS_SetPower(2, 0, 1);             // CLK2 2mA
    DSP_MeasureTrack(freq1, 1, 1, 1); //Fake initial run to let the circuit stabilize
                                      // inputMI = DSP_MeasuredTrackValue();
    Sleep(20);

    float xp = 1.0f;

    Cp = -1 / (6.2832 * freq1 * xp);
    if (selector == 1)
        return;

    deltaF = (BSVALUES_TRACK[span21] * 1000) / WWIDTH;
    nScanCount = CFG_GetParam(CFG_PARAM_PAN_NSCANS);

    for (i = 0; i <= WWIDTH; i++)
    {
        freq1 = fstart + i * deltaF;

        //DSP_MeasureTrack(freq1, 1, 1, nScanCount);
        //inputMI = -log10f(DSP_MeasuredTrackValue());

        valuesmI[i] = inputMI = CalcBin107(freq1, nScanCount, &value1, &value2, &value3);

        valuesdBI[i] = inputMI / 65.f * WHEIGHT;

        LCD_SetPixel(LCD_MakePoint(X0 + i, 135), LCD_BLUE); // progress line
        LCD_SetPixel(LCD_MakePoint(X0 + i, 136), LCD_BLUE);
    }

    FONT_Write(FONT_FRAN, LCD_RED, LCD_BLACK, 420, 0, "     ");
    GEN_SetMeasurementFreq(0);
    isMeasured = 1;
}
//=========================================================================
//                        NOT REMOVE BELOW LINES
//=========================================================================
////For Version 0.5
////Check memory interference and open
//int offsetIndex = 0;
//static void Scan21Fast_v05()
//{
//    float inputMI;
//    uint32_t i, nScanCount;
//    uint32_t fstart, freq1, deltaF;
//
//    f1=CFG_GetParam(CFG_PARAM_S21_F1);
//    if (CFG_GetParam(CFG_PARAM_PAN_CENTER_F) == 0)
//        fstart = f1;
//    else
//        fstart = f1 - 500*BSVALUES_TRACK[span]; // 2;
//
//    freq1 = fstart - 150000;
//    if (freq1 < 100000)
//        freq1 = 100000;
//
//    DSP_MeasureTrack(freq1, 1, 1, 1); //Fake initial run to let the circuit stabilize
//    inputMI = DSP_MeasuredTrackValue();
//    //Sleep(20);
//
//    DSP_MeasureTrack(freq1, 1, 1, 1);
//    inputMI = DSP_MeasuredTrackValue();
//
//    deltaF=(BSVALUES_TRACK[span] * 1000) / WWIDTH;
//    nScanCount = CFG_GetParam(CFG_PARAM_PAN_NSCANS);
//
///*
//#define MEASURE_INTERVAL_FAST 32
//#define USEHIST_INTERVAL_FAST 16
//#define MEASURE_MAX_INDEX 1
//*/
//#define MEASURE_INTERVAL_FAST 32
//#define USEHIST_INTERVAL_FAST 8
//#define MEASURE_MAX_INDEX 3
//
//    const int FAST_SCAN_OFFSET[] = {0, 16, 8, 24};
//
//    int fastOffset = FAST_SCAN_OFFSET[offsetIndex++];
//
//    if (offsetIndex > MEASURE_MAX_INDEX)
//        offsetIndex = 0;
//
//    for(i = 0; i <= WWIDTH; i++)
//    {
//        freq1 = fstart + (i + fastOffset) * deltaF;
//        if (freq1 == 0) //To overcome special case in DSP_Measure, where 0 is valid value
//            freq1 = 1;
//
//        int drawX = i + fastOffset;
//
//
//        if (i % MEASURE_INTERVAL_FAST != 0)
//        {
//            continue;
//        }
//#ifdef _DEBUG_UART
////    DBG_Printf("PROCESS: OFST:%d, IDX:%d, drawX:%d, Freq:%u", fastOffset, i, drawX, freq1);
//#endif
//
//        //DSP_MeasureTrack(freq1, 1, 1, nScanCount);
//        DSP_MeasureTrack(freq1, 1, 1, 1);
//        valuesdBI[drawX] = OSL_TXTodB(freq1, DSP_MeasuredTrackValue());
//
//            /*
//        //Break by Touch
//        if (autofast)
//        {
//            if (TOUCH_Poll(&pt))
//            {
//                if (pt.y > 230 && pt.x > 442)
//                {
//                    autofast = 0;
//                    TRACK_Beep(0);
//                    TRACK_DrawFootText();
//                    return;
//                }
//            }
//        }
//            */
//    }
//
//    //FONT_Write(FONT_FRAN, LCD_RED, LCD_BLACK, 420, 0, "     ");
//    GEN_SetMeasurementFreq(0);
//    isMeasured = 1;
//
///*
//    //fastOffset
//    //First Interpolate
//    for(i = fastOffset; i < WWIDTH - MEASURE_INTERVAL_FAST; i += USEHIST_INTERVAL_FAST)
//    {
//        uint32_t fr = i % MEASURE_INTERVAL_FAST;
//
//        if (fastOffset == fr)
//            continue;
//
//        int fi0, fi2;
//
//        //17 : 16, 48
//
//        fi0 = i / MEASURE_INTERVAL_FAST;
//        fi2 = fi0 + 1;
//
//        fi0 *= MEASURE_INTERVAL_FAST;
//        fi2 *= MEASURE_INTERVAL_FAST;
//
//        fi0 += fastOffset;
//        fi2 += fastOffset;
//
//        if (fi2 > WWIDTH)
//            continue;
//
//#ifdef _DEBUG_UART
//    //DBG_Printf("PROCESS: OFST:%d, IDX:%d, f0:%d, f2:%u", fastOffset, i, fi0, fi2);
//#endif
//
//        float Yi = valuesdBI[fi0];
//        float Xi = fi0;
//        float Yi1 = valuesdBI[fi2];
//        float Xi1 = fi2;
//        float Xb = i;
//
//        float Yb = Yi + (Yi1 - Yi) * (Xb - Xi) / (Xi1 - Xi);
//        valuesdBI[i] = Yb * 0.3 + valuesdBI[i] * 0.7;
//    }
//*/
//
//
//    //Interpolate intermediate values
//    for(i = 1; i <= WWIDTH; i++)
//    {
//        uint32_t fr = i % USEHIST_INTERVAL_FAST;
//
//        if (0 == fr)
//            continue;
//
//        int fi0, fi1, fi2;
//
//        fi0 = i / USEHIST_INTERVAL_FAST;
//        fi2 = fi0 + 1;
//
//        fi0 *= USEHIST_INTERVAL_FAST;
//        fi2 *= USEHIST_INTERVAL_FAST;
//
//
//        float Yi = valuesdBI[fi0];
//        float Xi = fi0;
//        float Yi1 = valuesdBI[fi2];
//        float Xi1 = fi2;
//        float Xb = i;
//
//        float Yb = Yi + (Yi1 - Yi) * (Xb - Xi) / (Xi1 - Xi);
//        valuesdBI[i] = Yb;
//    }
//
///*
//    //Full Interpolate by Latest Measurement data
//    for(i = fastOffset; i <= WWIDTH; i++)
//    {
//        uint32_t fr = i % MEASURE_INTERVAL_FAST;
//
//        if (fastOffset == fr)
//            continue;
//
//        int fi0, fi2;
//        fi0 = i / MEASURE_INTERVAL_FAST;
//        fi2 = fi0 + 1;
//
//        fi0 *= MEASURE_INTERVAL_FAST;
//        fi2 *= MEASURE_INTERVAL_FAST;
//
//        fi0 += fastOffset;
//        fi2 += fastOffset;
//
//
//        //if (fi2 > WWIDTH)
//        //    continue;
//
//#ifdef _DEBUG_UART
//    //DBG_Printf("PROCESS: OFST:%d, IDX:%d, f0:%d, f2:%u", fastOffset, i, fi0, fi2);
//#endif
//
//        float Yi = valuesdBI[fi0];
//        float Xi = fi0;
//        float Yi1 = valuesdBI[fi2];
//        float Xi1 = fi2;
//        float Xb = i;
//
//        float Yb = Yi + (Yi1 - Yi) * (Xb - Xi) / (Xi1 - Xi);
//        //valuesdBI[i] = Yb * 0.3 + valuesdBI[i] * 0.7;
//
//        if (fabs(valuesdBI[i] - Yb) > 20)
//        {
//            valuesdBI[i] = Yb ;
//        }
//        else
//        {
//            valuesdBI[i] = Yb * 0.3 + valuesdBI[i] * 0.7;
//        }
//        //valuesdBI[i] = Yb;
//    }
//*/
//
//}

//Version 0.35

static uint32_t fstart, freq1;

static void MakeFstart(void)
{
    f1 = CFG_GetParam(CFG_PARAM_S21_F1);
    span21 = CFG_GetParam(CFG_PARAM_S21_SPAN);
    if (CFG_GetParam(CFG_PARAM_PAN_CENTER_F) == 0)
        fstart = f1;
    else
        fstart = f1 - 500 * BSVALUES_TRACK[span21]; // 2;

    freq1 = fstart - 150000;
    if (freq1 < 100000)
        freq1 = 100000;
}

static void RedrawWindowS21(int justGraphDraw);
//static LCDPoint    pt1;
static int touchIndex;

void TrackTouchExecute(LCDPoint pt)
{

    touchIndex = GetTouchIndex(pt, trackMenus, trackMenu_Length);
    if (touchIndex < 9)
    { // Beep
        TRACK_Beep(1);
        while (TOUCH_IsPressed())
            ;
        Sleep(100);
    }

    switch (touchIndex)
    { // Exit
    case 0:
    {
        exitScan = 1;
        break;
    }
    case 9:
    { // < Button
        if (FirstRunS21 == 1)
        {
            touchIndex = 8;
            FirstRunS21 = 0;
            break;
        }
        //if(autofast==0)
        MoveCursor(-1);
        break;
    }
    case 10:
    { // > Button
        if (FirstRunS21 == 1)
        {
            touchIndex = 8;
            FirstRunS21 = 0;
            break;
        }
        //if(autofast==0)
        MoveCursor(1);
        break;
    }
    case 1:
    { //SCAN
        if (autofast)
            break;

        FONT_Write(FONT_FRANBIG, LCD_RED, LCD_BLACK, 180, 100, "  Scanning...  ");
        redrawRequired = 1;
        break;
    }
    case 2:
    { //ZOOM OUT
        //zoomMinus();
        if (fcur != 0)
            f1 = fcur * 1000.;
        if (span21 > 0)
            span21--;
        redrawRequired = 1;
        break;
    }
    case 3:
    { //ZOOM IN
        //zoomPlus();
        if (fcur != 0)
            f1 = fcur * 1000.;
        if (span21 < BS100M)
            span21++;
        redrawRequired = 1;
        break;
    }
    case 4: //Input Freq
    case 5:
    {
        touchIndex = 4;
        break;
    }
    case 6:
    { //Capture
        LCD_FillRect((LCDPoint){0, 248}, (LCDPoint){50, 271}, BackGrColor);
        save_snapshot();
        //Redraw
        //redrawRequired = 1;
        RedrawWindowS21(0);
        //DrawCursorS
        TRACK_DrawFootText();
        DrawAutoText();
        break;
    }
    case 7:
    { // SetCursor
        //if(autofast!=0) break;
        uint16_t cursorNew = pt.x;
        if (pt.x < X0)
            cursorNew = X0;
        if (pt.x > X0 + WWIDTH)
            cursorNew = WWIDTH;
        MoveCursor(cursorNew - cursorPos - X0);
        break;
    } //end of case
    }
    if (touchIndex == 8)
    {
        // Button Auto
        if (autofast == 0)
        {
            autofast = 1;
            sprintf(tmpBuff, "AA EU1KY-KD8CEC-DH1AKF AUTO: [%d] ", autoScanFactor);
            FONT_Write(FONT_FRAN, TextColor, BackGrColor, 0, 0, tmpBuff);
            Track_DrawGrid(2);
            cursorChangeCount = 0;
            LCD_FillRect(LCD_MakePoint(40, 15), LCD_MakePoint(459, 225), BackGrColor);
            Track_DrawGrid(1);
        }
        else
        {
            autofast = 0;
            FONT_Write(FONT_FRAN, TextColor, BackGrColor, 160, 0, "          ");
            StrictDrawCursor();
            TRACK_DrawFootText();
        }
        return;
    }
}

float LinInterpolation(uint32_t frequency)
{

    int Idx = GetIndexForFreq(frequency);

    uint32_t fr1 = OSL_GetCalFreqByIdx(Idx);

    float valdB01 = osl_txCorr[Idx].valAtt;

    if (frequency == fr1)
        return valdB01;

    uint32_t fr2 = OSL_GetCalFreqByIdx(Idx + 1);

    float valdB02 = osl_txCorr[Idx + 1].valAtt;

    if (valdB02 < 0.000000001f)
        return valdB01; // reject error values

    float prop = (float)(frequency - fr1) / (fr2 - fr1);

    return (valdB02 - valdB01) * prop + valdB01;
}

float CalcBin107(uint32_t frequency, int iterations, float *val1, float *val2, float *val3)
{             //
    float x1; //, x2;

    int Idx = GetIndexForFreq(frequency);

    int dB0 = CFG_GetParam(CFG_PARAM_ATTENUATOR);

    osl_txCorr = (OSL_ERRCORR *)WORK_Ptr;

    float valdB01 = osl_txCorr[Idx].val0;
    float valdB02 = osl_txCorr[Idx].valAtt;
    //float CorrInter = LinInterpolation(frequency);

    x1 = 10.f * log10f(DSP_GetValue(frequency, iterations));
    *val1 = 10.f * log10f(valdB01); // result is > 0 // red dotted line for test
    *val2 = 10.f * log10f(valdB02); // result is > 0
    //*val3=-x1+50.f;
    //return 5.f + (x1-*val1)*(30-5)/(*val2-*val1);
    if (*val2 < *val1)
        return (float)dB0 * (x1 - *val1) / (*val2 - *val1); //linear interpolation
    else
        return -x1 + 50.f; // ?????
}

/*
void SetMeasPointS21B(uint32_t i, int pos, LCDColor col){
int lmod = 5;
int k, length;
    if(pos<Y0) pos=Y0;
    if(pos>Y0+WHEIGHT) pos=Y0+WHEIGHT;

    LCD_SetPixel(LCD_MakePoint(i+X0, pos), col);// set the new pixel(s)
    if(FatLines){
        LCD_SetPixel(LCD_MakePoint(i+X0, pos-1), col);// WK
    }
}
*/

void SetMeasPointS21(uint32_t i, int pos)
{
    int r;
    int k; //, length;
    if (pos < Y0)
        pos = Y0;
    if (pos > Y0 + WHEIGHT)
        pos = Y0 + WHEIGHT;

    if (i == cursorPos)
        StrictDelCursor();

    c31 = LCD_ReadPixel(LCD_MakePoint(i + X0, Y0 + 31)); // no horizontal line between 30..39
    c35 = LCD_ReadPixel(LCD_MakePoint(i + X0, Y0 + 35));
    c = c31;
    if (c31 == CurvColor)
        c = c35;
    LCD_VLine(LCD_MakePoint(i + X0, Y0), WHEIGHT + 2, c); //draw original
    r = 10 * Y0;
    for (k = 0; k < 14; k++)
    { // horizontal lines
        LCD_SetPixel(LCD_MakePoint(i + X0, r / 10), WGRIDCOLOR);
        if (k % 2 == 0)
            LCD_SetPixel(LCD_MakePoint(i + X0, r / 10 + 1), WGRIDCOLOR);
        r += 146;
    }
    if (i == cursorPos)
        StrictDrawCursor();
    LCD_SetPixel(LCD_MakePoint(i + X0, pos), CurvColor); // set the new pixel(s) over cursor line
    if (FatLines)
    {
        LCD_SetPixel(LCD_MakePoint(i + X0, pos - 1), CurvColor);
        LCD_SetPixel(LCD_MakePoint(i + X0, pos + 1), CurvColor);
    }
}

static void Scan21Fast()
{
    LCDPoint pt;
    uint32_t i, k, m, nScanCount;
    uint32_t deltaF, returni = 0;
    int i0, i2;
    float newValue; //, newValue1,newValue2,newValue3;

    deltaF = (BSVALUES_TRACK[span21] * 1000) / WWIDTH;
    nScanCount = CFG_GetParam(CFG_PARAM_PAN_NSCANS);
    CLK2_drive = 0;
    HS_SetPower(2, 0, 1); // CLK2 2mA
    for (i = 0; i <= WWIDTH; i++)
    {
        freq1 = fstart + i * deltaF;
        k = i / autoScanFactor;
        if ((0 == (i % 80)) && TOUCH_Poll(&pt))
        { //  Break/Exit by Touch
            returni = 1;
            break;
        }

        if (i % autoScanFactor == 0)
        {
            valuesmI[i] = CalcBin107(freq1, nScanCount, &value1, &value2, &value3);
            newValue = valuesmI[i] / 65.f * WHEIGHT; ////////
                                                     /* newValue1 = value1/65.f*WHEIGHT;
            newValue2 = value2/65.f*WHEIGHT;
            newValue3 = value3/65.f*WHEIGHT;*/
            SetMeasPointS21(i, Y0 + newValue);
            /*SetMeasPointS21B(i, Y0+newValue1, LCD_COLOR_RED);// red dotted line for test
            SetMeasPointS21B(i, Y0+newValue2, LCD_COLOR_BLUE);// blue dotted line for test
            SetMeasPointS21B(i, Y0+newValue3, LCD_COLOR_YELLOW);// yellow dotted line for test (original value)*/
        }
        if (k >= 1)
        {
            i0 = k * autoScanFactor;
            i2 = i0 - autoScanFactor;
            for (m = i2 + 1; m < i0; m++)
            {

                //uint32_t fr1=OSL_GetCalFreqByIdx(i);
                //uint32_t fr2=OSL_GetCalFreqByIdx(i+1);
                //freq1=freq1+ deltaF*(i0-m)/autoScanFactor;
                //Interpolate previous intermediate values linear
                valuesmI[m] = valuesmI[i2] + (valuesmI[i0] - valuesmI[i2]) * (m - i2) / autoScanFactor;
                //valuesmI[m] = valuesmI[i2] +  (valuesmI[i0] - valuesmI[i2])*(freq1-fr1)/(fr2-fr1);
                newValue = valuesmI[m] / 65.f * WHEIGHT + Y0;
                SetMeasPointS21(m, newValue);
            }
        }
    }
    if (returni == 1)
        return;
    isMeasured = 1;
}

int lastoffset;

static void Track_DrawCurve(void) // SelQu=1, if quartz measurement  SelEqu=1, if equal scales
{
#define LimitR 1999.f
    int i; //, imax;
    int x, yofs;

    if (!isMeasured)
        return;
    Track_DrawGrid(1);

    for (i = 0; i <= WWIDTH; i++)
    {
        x = X0 + i;

        yofs = valuesmI[i] / 65.f * WHEIGHT + Y0;

        if (yofs > 208)
        {
            yofs = 208;
        }
        else if (yofs < Y0)
        {
            yofs = Y0;
        }
        LCD_SetPixel(LCD_MakePoint(x, yofs), CurvColor);
        if (FatLines)
        {
            LCD_SetPixel(LCD_MakePoint(x, yofs + 1), CurvColor); // WK
            LCD_SetPixel(LCD_MakePoint(x, yofs - 1), CurvColor); // WK
        }
    }
    StrictDrawCursor();
}

static void RedrawWindowS21(int justGraphDraw)
{
    Track_DrawGrid(justGraphDraw);
    Track_DrawCurve(); // 1

    if (!justGraphDraw)
    {
        TRACK_DrawFootText();
    }
    DrawMeasuredValues();
    DrawAutoText();
}

#define XX0 190
#define YY0 42

static void save_snapshot(void)
{
    //static const TCHAR *sndir = "/aa/snapshot";
    //char path[64];
    //char wbuf[256];
    char *fname = 0;
    //uint32_t i = 0;
    //FRESULT fr = FR_OK;

    if (!isMeasured)
        return;

    Date_Time_Stamp();

    fname = SCREENSHOT_SelectFileName();

    if (strlen(fname) == 0)
        return;

    SCREENSHOT_DeleteOldest();
    if (CFG_GetParam(CFG_PARAM_SCREENSHOT_FORMAT))
        SCREENSHOT_SavePNG(fname);
    else
        SCREENSHOT_Save(fname);

    //DrawSavedText();
    //static const char* txt = "  Snapshot saved  ";
    FONT_Write(FONT_FRAN, LCD_WHITE, LCD_RGB(0, 60, 0), 165, Y0 + WHEIGHT + 16 + 16, "  Snapshot saved  ");
    TRACK_DrawFootText();
    DrawAutoText();

    return;
}

//=====================================================================
//TRACK PROC
//---------------------------------------------------------------------
/*   ============= TEST =============
int exittr;
void raus(void){
    exittr=1;
}

void TestFunction(void){
float Test1, test2;
int i,k;
float valdB01, valdB02;

    exittr=0;
    k=0;
    LCD_FillAll(BackGrColor);
    osl_txCorr=(OSL_ERRCORR*)WORK_Ptr;
    if (! OSL_IsTXCorrLoaded())
        OSL_LoadTXCorr();
    if(TOUCH_IsPressed());
    while (exittr==0){
        for(i=0;i<6;i++){
            valdB01=osl_txCorr[k].val0;
            valdB02=osl_txCorr[k].valAtt;
            FONT_Print(FONT_FRAN, TextColor, BackGrColor, 20, 25*i, "t1: %g t2: %g",
               valdB01, valdB02);
               k++;
        }

        Sleep(5000);
        if(k>18) exittr=1;
    }

}

#define M_BGCOLOR LCD_RGB(0,0,64)    //Menu item background color
#define M_FGCOLOR LCD_RGB(255,255,0) //Menu item foreground color
#define COL1 10  //Column 1 x coordinate
TEXTBOX_CTX_t SWRT;
void Track_Proc1(void);

static const TEXTBOX_t tb_menut1[] = {
    (TEXTBOX_t){.x0 = COL1, .y0 = 10, .text =  " Exit ", .font = FONT_FRANBIG,.width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = raus , .cbparam = 1, .next = (void*)&tb_menut1[1] },
    (TEXTBOX_t){.x0 = COL1, .y0 = 60, .text =  " Continue ", .font = FONT_FRANBIG,.width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = Track_Proc1, .cbparam = 1, .next = (void*)&tb_menut1[2] },
    (TEXTBOX_t){.x0 = COL1, .y0 = 110, .text =    " Test  ", .font = FONT_FRANBIG,.width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = TestFunction , .cbparam = 1, .next = NULL },
};

void Track_Proc(void){
    exittr=0;
    LCD_FillAll(BackGrColor);
    TEXTBOX_InitContext(&SWRT);// double using
    TEXTBOX_Append(&SWRT, (TEXTBOX_t*)tb_menut1);
    TEXTBOX_DrawContext(&SWRT);
    while(exittr==0){
        if (TEXTBOX_HitTest(&SWRT))
        {
            if(exittr==1) break;
        };
    }
}

void Track_Proc1(void)
*/

void Track_Proc(void) // ==================== S21 screen ====================================
{
    uint32_t fxkHzs; //Scan range start frequency, in kHz
    uint32_t fxs;
    //extern int OSL_ENTRIES;

    redrawRequired = exitScan = 0;
    IsInverted = false;
    //allocate memory
    valuesmI = (float *)SDRH_malloc(sizeof(float) * (WWIDTH + 20));
    //valuesdBI = (float *)malloc(sizeof(float) * (WWIDTH + 20));
    activeLayerS21 = 1;
    BSP_LCD_SelectLayer(activeLayerS21);
    LCD_ShowActiveLayerOnly();
    cursorVisibleS21 = 0; // in the beginning not visible
    if (CFG_GetParam(CFG_PARAM_PAN_CENTER_F) == 1)
        cursorPos = WWIDTH / 2;
    else
        cursorPos = 0;
    SetColours();
    LCD_FillAll(BackGrColor);
    FONT_Write(FONT_FRAN, TextColor, BackGrColor, 0, 0, "AA EU1KY-KD8CEC-DH1AKF ");
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 230, 100, "|S21|Gain");
    Sleep(1000);
    while (TOUCH_IsPressed())
        ;
    //Load Calibration
    if (!OSL_IsTXCorrLoaded())
    {
        OSL_LoadTXCorr();
#ifdef _DEBUG_UART
        DBG_Str("Load VNA Calibration File \r\n");
#endif
    }
    if (!OSL_IsTXCorrLoaded())
        CRASH("No S21TX file");
    osl_txCorr = (OSL_ERRCORR *)WORK_Ptr;

    //Load saved frequency and span values from config file
    uint32_t fbkup = CFG_GetParam(CFG_PARAM_S21_F1);

    //2019.03.26
    //if (fbkup != 0 && fbkup >= BAND_FMIN && fbkup <= CFG_GetParam(CFG_PARAM_BAND_FMAX) && (fbkup % 100) == 0)
    if (fbkup != 0 && fbkup >= BAND_FMIN && (fbkup % 100) == 0)
    {
        f1 = fbkup;
    }
    else
        f1 = 14000000;
    CFG_SetParam(CFG_PARAM_S21_F1, f1);
    CFG_Flush();

    MakeFstart();
    span21 = CFG_GetParam(CFG_PARAM_S21_SPAN);
    autoScanFactor = CFG_GetParam(CFG_PARAM_S21_AUTOSPEED);

    Track_DrawGrid(0);
    TRACK_DrawFootText();
    DrawAutoText();
    while (!TOUCH_IsPressed())
        ;
    BSP_LCD_SelectLayer(activeLayerS21);
    LCD_FillAll(BackGrColor);
    BSP_LCD_SelectLayer((1 + activeLayerS21) % 2);
    LCD_FillAll(BackGrColor);
    Track_DrawGrid(0);
    BSP_LCD_SelectLayer(activeLayerS21);
    exitScan = 0;
    FirstRunS21 = 1;
    redrawRequired = 0;
    autofast = 0;

    for (;;)
    {
        Sleep(0);
        if (TOUCH_Poll(&pt0))
        { // touch check
            TrackTouchExecute(pt0);
            if (exitScan == 1)
                break;
        }
        else
        {
            cursorChangeCount = 0;
            trackbeep = 0;
        }                    //end of
        if (touchIndex == 4) // new frequency
        {
            fxs = CFG_GetParam(CFG_PARAM_S21_F1);
            fxkHzs = fxs / 1000;
            if (PanFreqWindow(&fxkHzs, (BANDSPAN *)&span21))
            {
                f1 = fxkHzs * 1000;
                CFG_SetParam(CFG_PARAM_S21_F1, f1);
                CFG_SetParam(CFG_PARAM_S21_SPAN, span21);
                CFG_Flush();
                MakeFstart(); // and span
                isMeasured = 0;
                Track_DrawGrid(2);
                /* BSP_LCD_SelectLayer((1+activeLayerS21)%2);
                Track_DrawGrid(2);
                BSP_LCD_SelectLayer(activeLayerS21);*/
                redrawRequired = 0;
                IsInverted = false;
            }
            touchIndex = 255;
        }

        if (autofast) //Auto Scan
        {

            /* activeLayerS21=(1+activeLayerS21)%2;
            BSP_LCD_SelectLayer(activeLayerS21);
            BSP_LCD_SetLayerVisible_NoReload(activeLayerS21, 1);//  ?
            BSP_LCD_SetLayerVisible((1+activeLayerS21)%2, 0);*/
            Scan21Fast();
            DrawMeasuredValues();
            /* BSP_LCD_SetLayerVisible_NoReload(activeosl_txCorrLayerS21, 1);//  ?
            BSP_LCD_SetLayerVisible((1+activeLayerS21)%2, 0);*/
            redrawRequired = 0;
        }
        if (redrawRequired)
        {
            Scan21(0);
            RedrawWindowS21(0);
            redrawRequired = 0;
        }
        LCD_ShowActiveLayerOnly();
    }                   //end of for(;;)
    GEN_SetClk2Freq(0); // CLK2 off
    //Release Memory
    SDRH_free(valuesmI);
    isMeasured = 0;
}
