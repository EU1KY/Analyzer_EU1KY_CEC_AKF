#ifndef _TEXTBOX_H_
#define _TEXTBOX_H_

#include <stdint.h>
#include "font.h"

typedef enum
{
    TEXTBOX_TYPE_TEXT = 0, //Visible item with text
    TEXTBOX_TYPE_BMP,      //Visible item with bitmap. User MUST provide correct bmp, bmpsize, width and height parameters. Only 8BPP bitmaps are supported!
    TEXTBOX_TYPE_HITRECT,  //Invisible hit rectangle
} TEXTBOX_TYPE_t;

#pragma pack(push, 1)
typedef struct
{
    uint8_t type : 3;    //TEXTBOX_TYPE_t
    uint8_t border : 1;  //Set to 1 to draw border
    uint8_t cbparam : 1; //Set to 1 to use callback with parameter
    uint8_t center : 1;  //Set to 1 to center text in the box with predefined width and height
    uint8_t nowait;      //Set to nonzero to bypass waiting for touch release and to return 0 from hit test func
    uint16_t x0;         //Origin x
    uint16_t y0;         //Origin y
    union
    {
        const char *text;   //Text of the box for TEXTBOX_TYPE_TEXT type
        const uint8_t *bmp; //Pointer to 8BPP bitmap file placed in memory, for TEXTBOX_TYPE_BMP type
    };
    union
    {
        uint32_t font;    //Font of the box for TEXTBOX_TYPE_TEXT type
        uint32_t bmpsize; //Bitmap data size in bytes for TEXTBOX_TYPE_BMP type
    };
    uint32_t fgcolor; //Foreground color for TEXTBOX_TYPE_TEXT type
    uint32_t bgcolor; //Background color for TEXTBOX_TYPE_TEXT type
    void (*cb)(void); //Callback function to be called when textbox is tapped
                      //   uint16_t val;            // Callback value  WK
    uint16_t width;   //Filled automatically for TEXTBOX_TYPE_TEXT type
    uint16_t height;  //Filled automatically for TEXTBOX_TYPE_TEXT type
    void *next;       //Filled automatically in TEXTBOX_Append
} TEXTBOX_t;
#pragma pack(pop)

typedef struct
{
    TEXTBOX_t *start;
} TEXTBOX_CTX_t;

void TEXTBOX_InitContext(TEXTBOX_CTX_t *ctx);
uint32_t TEXTBOX_Append(TEXTBOX_CTX_t *ctx, TEXTBOX_t *hbox);
void TEXTBOX_DrawContext(TEXTBOX_CTX_t *ctx);
void TEXTBOX_Clear(TEXTBOX_CTX_t *ctx, uint32_t idx);
void TEXTBOX_SetText(TEXTBOX_CTX_t *ctx, uint32_t idx, const char *txt);
uint32_t TEXTBOX_HitTest(TEXTBOX_CTX_t *ctx);
TEXTBOX_t *TEXTBOX_Find(TEXTBOX_CTX_t *ctx, uint32_t idx);

#endif //_TEXTBOX_H_
