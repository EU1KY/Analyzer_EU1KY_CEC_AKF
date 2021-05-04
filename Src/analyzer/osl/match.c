/*
 *   (c) Wolfgang Kiefer
 *   dh1akf@darc.de
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "match.h"
#include "LCD.h"
#include "font.h"
#include "dsp.h"
#include "config.h"

#define NORESULT 1000000.f

uint32_t fHz;

void PrintValue(float X, char* str){
    if (X == NORESULT)
    {
        strcpy(str, "     --- ");
    }
    else if ((0.f == X) || (-0.f == X))
    {
        strcpy(str, "        0");
    }
	else{
		if (X < 0.f)
		{
			float CpF = -1e12f / (2.f * M_PI * fHz * X);
			sprintf(str, " %6.1f pF", CpF);
		}
		else
		{
			float LuH = (1e6f * X) / (2.f * M_PI * fHz);
			sprintf(str, "%6.3f uH",  LuH);
		}
	}

}

int PrintLine(int X0, int Y0, float k1, float k2, float k3){
char str1[32];
char str2[32];
char str3[32];
	PrintValue(k1, str1);
	PrintValue(k2, str2);
	PrintValue(k3, str3);
	FONT_Print(FONT_FRAN, LCD_WHITE, LCD_BLACK, X0, Y0, "%8s   %8s   %8s", str1, str2, str3);
	return Y0 + FONT_GetHeight(FONT_FRAN) + 4;

}

// Calculate two solutions for ZL where R + X * X / R > R0
int calc_hi(int X0, int Y0, float R0, float complex ZL){
    float R = crealf(ZL);
    float X = cimagf(ZL);
    int Ynew = 0.f;
	float m1, m2, m3, m4, m5, m6;
    int ret = 0;
    if (R0 == R) return Y0;
    float e = X * R0 / (R - R0);
    float f = sqrt(R * R0) / (R - R0);
    float h = R * R - R * R0 + X * X;
    if (h < 0) return Y0;
    h = sqrt(h);
    float p = e + f * h;
    float q = e - f * h;
    float s = (R * R * p + p * X * X + p * p * X) / ((X + p) * (X + p) + R * R);
    m1 = m2 = m3 = m4 = m5 = m6 = NORESULT;
    if ((p < 0) && (q < 0)){//case 11B
        m1 = q;// capacitor SourceParallel
        m2 = s;// inductor Serial
        ret= 2;
    }
    else if ((p > 0) && (q > 0)) {//case 12D
        m1 = p;// inductor SourceParallel
        m2 = -s;// capacitor Serial
        ret = 4;
    }
    else if ((p > 0) && (q < 0)) {
        m1 = q;// capacitor SourceParallel case 11B
        m2 = s;// inductor Serial
        ret = 6;
        // 2nd solution:                case 12D
        m4= p;// inductor SourceParallel
        m5 = -s;// capacitor Serial
    }
    if (ret != 0) {
        Ynew = PrintLine(X0, Y0, m1, m2, m3);
    }
    if (ret == 6) {
        Ynew = PrintLine(X0, Ynew, m4, m5, m6);
    }
    return Ynew;
}

// Calculate two solutions for ZL where R < R0
int calc_lo(int X0, int Y0, float R0, float complex ZL){
float R = crealf(ZL);
float X = cimagf(ZL);
float m1, m2, m3, m4, m5, m6;
int ret = 0;
int Ynew = 0.f;
    m1 = m2 = m3 = m4 = m5 = m6 = NORESULT;
    if (R0 == R) {
         m2 = -X;// Ser = -X compensates X
         ret = 7;
    }
	else{
		if (R0 < R) return Y0;

		float a = sqrt(R * (R0 - R)) - X;
		float b = R0 * sqrt(R / (R0 - R));
		float c = -2 * X - a;

		if ((a < 0) && (c < 0) && (a < c)) {// case 1C
			 m2 = c;// a < 0 capacitor Serial
			 m3 = -a;// inductor LoadParallel
			 ret = 3;
		}
		else if ((a < 0) && (c < 0) && (a >= c)) { // case 4C
			 m2 = c;// capacitor Serial
			 m3 = b;// inductor LoadParallel
			 ret = 3;
		}
		else if ((a > 0) && (c > 0)) {// case 2A
			 m2 = a;// inductor Serial
			 m3 = -b;//capacitor LoadParallel
			 ret = 1;
		}
		if ((a >= 0) && (c <= 0)) {// case 3C + 3A
			 m2 = c;// capacitor Serial
			 m3 = b;// inductor  LoadParallel
			 ret = 5;
			 ;// 2nd variant:
			 m5 = a;// inductor Serial
			 m6 = -b;//capacitor LoadParallel
		}
	}
	if (ret != 0)
		Ynew = PrintLine(X0, Y0, m1, m2, m3);
	if(ret==5)
		Ynew = PrintLine(X0, Ynew, m4, m5, m6);
	return Ynew;
}

void MATCH_Calc(int X0, int Y0, float complex ZL){
int Ynew = 0;
float vswr = DSP_CalcVSWR(ZL);
    if (vswr <= 1.05f || vswr >= 50.f){
			//Don't calculate for low and too high VSWR
		FONT_Write(FONT_FRAN, LCD_WHITE, LCD_BLACK, X0, Y0, "No LC match for this load");
		return;
	}
	fHz = CFG_GetParam(CFG_PARAM_MEAS_F);// global variable
    float R0 = (float)CFG_GetParam(CFG_PARAM_R0);
    if ((crealf(ZL) > (0.91f * R0)) && (crealf(ZL) < (1.1f * R0)))
    {//Only one solution is enough: just a serial reactance, this gives SWR < 1.1 if R is within the range 0.91 .. 1.1 of R0
		PrintLine(X0, Y0, NORESULT, NORESULT, -cimagf(ZL));
        return;
    }
			//Calc Lo-Z solutions here
    Ynew = calc_lo(X0, Y0, R0, ZL);
			// Calc the Hi-Z solutions
    Ynew = calc_hi(X0, Ynew, R0, ZL);
}
