/*
 *   (c) Ralf Figge
 *   dg2drf@dc0dam.de
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
  ='\0';
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ctype.h"
#include "stdlib.h"
#include "bt.h"
#include "touch.h"
#include "aauart.h"
#include "config.h"
#include "font.h"
#include "lcd.h"
#include "keyboard.h"
#include "textbox.h"

#define M_BGCOLOR LCD_RGB(0,0,64)    //Menu item background color
#define M_FGCOLOR LCD_RGB(255,255,0) //Menu item foreground color

#define COL1 10  //Column 1 x coordinate
#define COL2 230 //Column 2 x coordinate

extern void Sleep(uint32_t);
static uint32_t rqExit = 0;
static TEXTBOX_CTX_t menuBT_ctx;
static const char *BT_AT_TEST  =   "AT";
static const char *BT_AT_VERSION = "AT+VERSION";
static const char *BT_AT_NAME  =   "AT+NAME";
static const char *BT_AT_PIN    =  "AT+PIN";
// Baudraten
static const char *BT_BAUD_1200   =  "AT+BAUD1";
static const char *BT_BAUD_2400   =  "AT+BAUD2";
static const char *BT_BAUD_4800   =  "AT+BAUD3";
static const char *BT_BAUD_9600   =  "AT+BAUD4";
static const char *BT_BAUD_19200  =  "AT+BAUD5";
static const char *BT_BAUD_38400  =  "AT+BAUD6";
static const char *BT_BAUD_57600  =  "AT+BAUD7";
static const char *BT_BAUD_115200 =  "AT+BAUD8";





#define BTB_SIZE 64
static char btstr[BTB_SIZE+1];
static uint32_t bt_ctr = 0;
static char* btstr_p;

static char g_bt_data[29]   = {0};
char *BT_SSID               = &g_bt_data[0];
uint32_t BT_PIN             =  (uint32_t) &g_bt_data[22];                //0

#include "ff.h"
#include "crash.h"
static const char *g_bt_fpath = "/aa/bt.bin";
static char* NewSSID;


static void _hit_BTexit(void)
{
    rqExit = 1;
}

static char* _del_command(char *str)
{
    char* end;
    if (str[0] == '+')
    {

    }
return end;
}

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

static int PROTOCOL_BTCmd(void)
{
    int ch = AAUART_Getchar();
    if (ch == EOF)
        return 0;
    else if (ch == '\r' || ch == '\n')
    {
        // command delimiter
        if (!bt_ctr)
            btstr[0] = '\0';
        bt_ctr = 0;
        return 1;
    }
    else if (ch == '\0') //ignore it
        return 0;
    else if (ch == '\t')
        ch = ' ';
    else if (bt_ctr >= (BTB_SIZE - 1)) //skip all characters that do not fit in the rx buffer
        return 0;
    //Store character to buffer
    btstr[bt_ctr++] = (char)(ch & 0xFF);
    btstr[bt_ctr] = '\0';
    return 0;
}



void BT_StoredInformation(void)
{
    FRESULT res;
    FIL fo = { 0 };
    res = f_open(&fo, g_bt_fpath, FA_OPEN_ALWAYS | FA_WRITE);
    if (FR_OK == res)
    {
        UINT bw;
        res = f_write(&fo, g_bt_data, sizeof(g_bt_data), &bw);
        res = f_close(&fo);
    }
}


void BT_LoadInformation(void)
{
    FRESULT res;
    FIL fo = { 0 };

    FILINFO finfo;
    res = f_stat(g_bt_fpath, &finfo);
    if (FR_NOT_ENABLED == res || FR_NOT_READY == res)
        CRASH("No SD card");

    if (FR_OK == res)
    {
        res = f_open(&fo, g_bt_fpath, FA_READ);

        UINT br;
        f_read(&fo, g_bt_data, sizeof(g_bt_data), &br);
        f_close(&fo);
    }

 /*   //Default Setting
    if ((*BT_SSID = "") || (*BT_PIN = 0) || (*BT_Baud < 9600 ))  //Not Load Configuration
    {
        *BT_SSID             = "HC-05" ;                //0
        *BT_PIN              = 1234    ;    //20-23
        *BT_Baud             = 9600    ;    //24-27
    }

*/


}


void BT_init(void)
{
    uint32_t comport = CFG_GetParam(CFG_PARAM_COM_PORT);
    uint32_t comspeed = CFG_GetParam(CFG_PARAM_COM_SPEED);
    if (COM1 == comport)
       CFG_SetParam(CFG_PARAM_COM_PORT, COM2);
    else
    return;
  if ( CFG_GetParam(CFG_PARAM_COM_SPEED)!=  CFG_GetParam(CFG_PARAM_BT_SPEED))
     CFG_SetParam(CFG_PARAM_COM_SPEED,CFG_GetParam(CFG_PARAM_BT_SPEED));
  else
  return;

CFG_Flush;
BT_StoredInformation;
AAUART_Init;
}
extern uint32_t NumKeypad(uint32_t initial, uint32_t min_value, uint32_t max_value, const char* header_text);

void BT_ChangePW(void)
{
uint16_t changepw;
   changepw = 9999;
   while (TOUCH_IsPressed());// wait for release
   LCD_FillAll(LCD_BLACK);
   changepw=NumKeypad(changepw, 0000, 9999, "Bluetooth Password");
rqExit=1;
}



void BT_ChangeSID(void)
{
uint32_t changepw;
   while(TOUCH_IsPressed());// wait for release
   LCD_FillAll(LCD_BLACK);
   changepw=KeyboardWindow(NewSSID,20,"Change SSID");
rqExit=1;
}

static char* BT_Command(const char* str)
{
 AAUART_PutString(str);
 if (!PROTOCOL_BTCmd())
     return "ERROR";
 btstr_p = _trim(btstr);
 return btstr_p;
}

void ShowBluetooth(void){
char str1[30];
BT_SSID=BT_Command("AT+NAME?");
BT_PIN=9999;
    sprintf(str1,"SSID : %s", BT_SSID);
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 34, 10, str1);

    sprintf(str1,"PW   : %4d ", BT_PIN);
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 34, 40, str1);//60
    sprintf(str1,"Baud : %d ", CFG_GetParam(CFG_PARAM_BT_SPEED));
    FONT_Write(FONT_FRANBIG, TextColor, BackGrColor, 34, 75, str1);

    /*if  (Volt_max_Display==0) LCD_Rectangle(LCD_MakePoint(COL2,210),LCD_MakePoint(COL2+240,210+34),LCD_RED);
    else */ // LCD_Rectangle(LCD_MakePoint(COL2,210),LCD_MakePoint(COL2+240,210+34),LCD_YELLOW);
}


static const TEXTBOX_t BT_menu[] = {
//   (TEXTBOX_t){.x0 = COL1, .y0 = 10, .text =  " -0.01 ", .font = FONT_FRANBIG,.width = 90, .height = 34, .center = 1,
//                .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = minus10 , .cbparam = 1, .next = (void*)&BT_menu[1] },
//   (TEXTBOX_t){.x0 = COL3, .y0 = 10, .text =  " +0.01 ", .font = FONT_FRANBIG,.width = 90, .height = 34, .center = 1,
//                .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = plus10 , .cbparam = 1, .next = (void*)&BT_menu[2] },
//   (TEXTBOX_t){.x0 = COL1, .y0 = 60, .text =  " -0.1 ", .font = FONT_FRANBIG,.width = 90, .height = 34, .center = 1,
//                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = minus100 , .cbparam = 1, .next = (void*)&BT_menu[3] },
//    (TEXTBOX_t){.x0 = COL3, .y0 = 60, .text =  " +0.1 ", .font = FONT_FRANBIG,.width = 90, .height = 34, .center = 1,
//                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = plus100 , .cbparam = 1, .next = (void*)&BT_menu[1] },
    (TEXTBOX_t){.x0 = COL1, .y0 = 110, .text =  " Set SSID ", .font = FONT_FRANBIG,.width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = BT_ChangeSID , .cbparam = 1, .next = (void*)&BT_menu[1] },
//    (TEXTBOX_t){.x0 = COL3, .y0 = 110, .text =  " +1.0 ", .font = FONT_FRANBIG,.width = 90, .height = 34, .center = 1,
//                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = plus1000 , .cbparam = 1, .next = (void*)&BT_menu[2] },
//    (TEXTBOX_t){.x0 = 110, .y0 = 110, .text =    "Set Max Value", .font = FONT_FRANBIG,.width = 220, .height = 34, .center = 1,
//                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = SetMax , .cbparam = 1, .next = (void*)&BT_menu[3] },
    (TEXTBOX_t){.x0 = COL1, .y0 = 160, .text =    " Set Password  ", .font = FONT_FRANBIG,.width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = BT_ChangePW , .cbparam = 1, .next = (void*)&BT_menu[2] },
    (TEXTBOX_t){.x0 = COL2, .y0 = 160, .text =  " Set Baud ", .font = FONT_FRANBIG,.width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = BT_ChangePW , .cbparam = 1, .next = (void*)&BT_menu[3] },
//    (TEXTBOX_t){ .x0 = COL2, .y0 = 210, .text = "Voltage Display Off", .font = FONT_FRANBIG, .width = 240, .height = 34, .center = 1,
//                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = M_BGCOLOR, .cb = VoltOff , .cbparam = 1,.next = (void*)&BT_menu[6]},
    (TEXTBOX_t){ .x0 = COL1, .y0 = 210, .text = " Main Menu ", .font = FONT_FRANBIG, .width = 200, .height = 34, .center = 1,
                 .border = 1, .fgcolor = M_FGCOLOR, .bgcolor = LCD_RED, .cb = _hit_BTexit, .cbparam = 1,},
};




void MenuBT(void){
    while(TOUCH_IsPressed());
//    rqExit3=0;
//    VoltCase=0;
    LCD_FillAll(LCD_BLACK);
    TEXTBOX_InitContext(&menuBT_ctx);
    TEXTBOX_Append(&menuBT_ctx, (TEXTBOX_t*)BT_menu);
    TEXTBOX_DrawContext(&menuBT_ctx);
//    Volt_max_Display=CFG_GetParam(CFG_PARAM_Volt_max_Display);
//    Volt_min_Display=CFG_GetParam(CFG_PARAM_Volt_min_Display);
    ShowBluetooth();
 for(;;)
    {
        Sleep(0); //for autosleep to work
        if (TEXTBOX_HitTest(&menuBT_ctx))
        {
            Sleep(0);
            ShowBluetooth();
  /*          if (rqExit3==1)
            {
                rqExit3=0;
 //               CFG_SetParam(CFG_PARAM_Volt_max_Display,Volt_max_Display);
 //               CFG_SetParam(CFG_PARAM_Volt_min_Display,Volt_min_Display);
                CFG_Flush();
                rqExitR=true;
                return;
            }
            Sleep(50);*/
        }
        Sleep(0);
    }

}
