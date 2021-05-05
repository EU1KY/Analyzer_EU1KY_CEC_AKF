#include "touch.h"
#include "stm32746g_discovery_ts.h"
#include "stm32746g_discovery_lcd.h"
#include "lcd.h"
#include "config.h"

void TOUCH_Init(void)
{
    BSP_TS_Init(FT5336_MAX_WIDTH, FT5336_MAX_HEIGHT);
}

static uint32_t wakeup_touch(void)
{
    extern volatile uint32_t autosleep_timer;
    autosleep_timer = CFG_GetParam(CFG_PARAM_LOWPWR_TIME);
    if (LCD_IsOff())
    {
        BSP_LCD_DisplayOn();
        TS_StateTypeDef ts = {0};
        do
        {
            BSP_TS_GetState(&ts);
        } while (ts.touchDetected);
        return 1;
    }
    return 0;
}

uint8_t TOUCH_Poll(LCDPoint *pCoord)
{
    TS_StateTypeDef ts = {0};
    BSP_TS_GetState(&ts);
    if (ts.touchDetected)
    {
        if (wakeup_touch())
            return 0;

        if (LCD_Get_Orientation() == 1)
        {
            pCoord->x = 479 - ts.touchX[0];
            pCoord->y = 271 - ts.touchY[0];
        }
        else
        {
            pCoord->x = ts.touchX[0];
            pCoord->y = ts.touchY[0];
        }
    }
    return ts.touchDetected;
}

uint8_t TOUCH_IsPressed(void)
{
    TS_StateTypeDef ts = {0};
    BSP_TS_GetState(&ts);
    if (ts.touchDetected)
    {
        if (wakeup_touch())
            return 0;
    }
    return ts.touchDetected;
}

//KD8CEC
int GetTouchIndex(LCDPoint pt1, const int checkButtons[][4], int checkCount)
{
    // #define TOUCH_TOP_MARGIN 5
#define TOUCH_TOP_MARGIN 0

    for (int i = 0; i < checkCount; i++)
    {
        if (pt1.x >= checkButtons[i][BUTTON_LEFT] && pt1.x <= checkButtons[i][BUTTON_RIGHT] &&
            pt1.y + TOUCH_TOP_MARGIN >= checkButtons[i][BUTTON_TOP] && pt1.y <= checkButtons[i][BUTTON_BOTTOM])
        {

            return i;
        }
    }
    return -1;
}

int GetTouchIndex1(LCDPoint pt1, const int checkButtons[][4], int checkCount)
{
    for (int i = 0; i < checkCount; i++)
    {
        if (pt1.x >= checkButtons[i][BUTTON_LEFT] && pt1.x <= checkButtons[i][BUTTON_RIGHT] &&
            pt1.y + TOUCH_TOP_MARGIN >= checkButtons[i][BUTTON_TOP] && pt1.y <= checkButtons[i][BUTTON_BOTTOM])
        {

            return i;
        }
    }
    return -1;
}
