/*
 *   KD8CEC
 *   EU1KY code from mainwind.c
 *
 *   Modified : by KD8CEC , Added feather for DEBUG
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 *   Modified by VE7IT to add another simple remote serial protocol as used by some
 *   N2PK analyzer and linsmith.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "aauart.h"
#include "dsp.h"
#include "config.h"
#include "ctype.h"
#include "stdlib.h"
#include "gen.h"

//==========================================================================================
// Serial remote control protocol state machine implementation (emulates RigExpert AA-170)
// Runs in main window only
//==========================================================================================
#define RXB_SIZE 64

extern void Sleep(uint32_t);

static char rxstr[RXB_SIZE+1];
static uint32_t rx_ctr = 0;
static char* rxstr_p;
static uint32_t _fCenter = 10000000ul;
static uint32_t _fSweep = 100000ul;
static const char *ERR = "ERROR\r";
static const char *OK = "OK\r";

static char* _trim(char *str)
{
    char *end;
    while(isspace((int)*str))
        str++;
    if (*str == '\0')
        return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((int)*end))
        end--;
    *(end + 1) = '\0';
    return str;
}

static int PROTOCOL_RxCmd(void)
{
    int ch = AAUART_Getchar();
    if (ch == EOF)
        return 0;
    else if (ch == '\r' || ch == '\n')
    {
        // command delimiter
        if (!rx_ctr)
            rxstr[0] = '\0';
        rx_ctr = 0;
        return 1;
    }
    else if (ch == '\0') //ignore it
        return 0;
    else if (ch == '\t')
        ch = ' ';
    else if (rx_ctr >= (RXB_SIZE - 1)) //skip all characters that do not fit in the rx buffer
        return 0;
    //Store character to buffer
    rxstr[rx_ctr++] = (char)(ch & 0xFF);
    rxstr[rx_ctr] = '\0';
    return 0;
}

void PROTOCOL_Reset(void)
{
    rxstr[0] = '\0';
    rx_ctr = 0;
    //Purge contents of RX buffer
    while (EOF != AAUART_Getchar());
}


// added by VE7IT to support linsmith serial protocol used in some N2PK analyzers
// very simple.... when sent a frequency, return freq, realRx and imagRx as a string
void PROTOCOL_Handler_LINSMITH(void)
{
    char txstr[64];                 // uart tx string buffer
    DSP_RX rx;                      // impedance reading buffer
    unsigned int fint = 10000000u;  // 10MHz default freq

    if (!PROTOCOL_RxCmd())          // check for available cmds
        return;

    // Command received
    rxstr_p = _trim(rxstr);
    strlwr(rxstr_p);                // string to lower case

    // empty commands send back a default reading at 10MHz
    // linsmith expects a response as a test for analyzer present
    if(isdigit((int)rxstr_p[0]))    // if a number it could be a freq.
        fint = (uint32_t)atoi(&rxstr_p[0]);  //will be 0 if conversion error

    DSP_Measure(fint, 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
    rx = DSP_MeasuredZ();

    while(AAUART_IsBusy())
        Sleep(0); //prevent overwriting the data being transmitted
    sprintf(txstr, "%u %f %f\r\n",fint, crealf(rx), cimagf(rx));
    AAUART_PutString(txstr);
    return;
}

void PROTOCOL_Handler_AA600(void)
{
    if (!PROTOCOL_RxCmd())
        return;
    //Command received
    rxstr_p = _trim(rxstr);
    strlwr(rxstr_p);

    if (rxstr_p[0] == '\0') //empty command
    {
        AAUART_PutString(ERR);
        return;
    }

    if (0 == strcmp("ver", rxstr_p))
    {
        AAUART_PutString("AA-600 401\r");
        return;
    }

    if (0 == strcmp("on", rxstr_p))
    {
        GEN_SetMeasurementFreq(GEN_GetLastFreq());
        AAUART_PutString(OK);
        return;
    }

    if (0 == strcmp("off", rxstr_p))
    {
        GEN_SetMeasurementFreq(0);
        AAUART_PutString(OK);
        return;
    }

    if (rxstr_p == strstr(rxstr_p, "am"))
    {//Amplitude setting
        AAUART_PutString(OK);
        return;
    }

    if (rxstr_p == strstr(rxstr_p, "ph"))
    {//Phase setting
        AAUART_PutString(OK);
        return;
    }

    if (rxstr_p == strstr(rxstr_p, "de"))
    {//Set delay
        AAUART_PutString(OK);
        return;
    }

    if (rxstr_p == strstr(rxstr_p, "fq"))
    {
        uint32_t FHz = 0;
        if(isdigit((int)rxstr_p[2]))
        {
            FHz = (uint32_t)atoi(&rxstr_p[2]);
        }
        else
        {
            AAUART_PutString(ERR);
            return;
        }

        if (FHz < BAND_FMIN)
        {
            AAUART_PutString(ERR);
        }
        else
        {
            _fCenter = FHz;
            AAUART_PutString(OK);
        }
        return;
    }

    if (rxstr_p == strstr(rxstr_p, "sw"))
    {
        uint32_t sw = 0;
        if(isdigit((int)rxstr_p[2]))
        {
            sw = (uint32_t)atoi(&rxstr_p[2]);
        }
        else
        {
            AAUART_PutString(ERR);
            return;
        }
        _fSweep = sw;
        AAUART_PutString(OK);
        return;
    }

    if (rxstr_p == strstr(rxstr_p, "frx"))
    {
        uint32_t steps = 0;
        uint32_t fint;
        uint32_t fstep;

        if(isdigit((int)rxstr_p[3]))
        {
            steps = (uint32_t)atoi(&rxstr_p[3]);
        }
        else
        {
            AAUART_PutString(ERR);
            return;
        }
        if (steps == 0)
        {
            AAUART_PutString(ERR);
            return;
        }
        if (_fSweep / 2 > _fCenter)
            fint = 10;
        else
            fint = _fCenter - _fSweep / 2;
        fstep = _fSweep / steps;
        steps += 1;
        char txstr[64];
        while (steps--)
        {
            DSP_RX rx;

            DSP_Measure(fint, 1, 1, CFG_GetParam(CFG_PARAM_MEAS_NSCANS));
            rx = DSP_MeasuredZ();
            while(AAUART_IsBusy())
                Sleep(0); //prevent overwriting the data being transmitted
            sprintf(txstr, "%.6f,%.2f,%.2f\r", ((float)fint) / 1000000., crealf(rx), cimagf(rx));
            AAUART_PutString(txstr);
            fint += fstep;
        }
        AAUART_PutString(OK);

        return;
    }
    AAUART_PutString(ERR);
}

void PROTOCOL_Handler(void)
{
    switch (CFG_GetParam(CFG_PARAM_SEREMUL))
    {
    case 0:
        PROTOCOL_Handler_AA600();
        break;
    case 1:
        PROTOCOL_Handler_LINSMITH();
        break;
    default:
        CFG_SetParam(CFG_PARAM_SEREMUL,0);  // next call will default to something valid
    }
 }
