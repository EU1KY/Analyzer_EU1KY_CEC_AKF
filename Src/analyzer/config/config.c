#include "config.h"
#include "ff.h"
#include "crash.h"
#include "gen.h"
#include <string.h>
#include <stdint.h>

static uint32_t g_cfg_array[CFG_NUM_PARAMS] = {0};
const char *g_aa_dir = "/aa";
static const char *g_cfg_dir = "/aa/config";
static const char *g_cfg_fpath = "/aa/config/config.bin";
const char *g_cfg_osldir = "/aa/osl";
static uint32_t resetRequired = 0;
uint8_t ColourSelection;
bool FatLines;
int BeepOn1;
static uint32_t rqExit;
uint32_t BackGrColor;
uint32_t CurvColor;
uint32_t TextColor;
uint32_t Color1;
uint32_t Color2;
uint32_t Color3;
uint32_t Color4;
uint32_t Color5;

typedef enum
{
    CFG_PARAM_T_U8,  //8-bit unsigned
    CFG_PARAM_T_U16, //16-bit unsigned
    CFG_PARAM_T_U32, //32-bit unsigned
    CFG_PARAM_T_S8,  //8-bit signed
    CFG_PARAM_T_S16, //16-bit signed
    CFG_PARAM_T_S32, //32-bit signed
    CFG_PARAM_T_F32, //32-bit float
    CFG_PARAM_T_CH,  //char**[]
} CFG_PARAM_TYPE_t;

typedef struct
{
    CFG_PARAM_t id;            //ID of the configuration parameter, see CFG_PARAM_t
    const char *idstring;      //Short parameter name to be displayed
    uint32_t nvalues;          //Number of values in allowed values array. Can be 0 if not relevant.
    const int32_t *values;     //Array of integer values that can be selected for parameter. Length is specified in .values
    const char **strvalues;    //Array of alternative string representations for values that can be selected for parameter. Length of the array must be in .values
    CFG_PARAM_TYPE_t type;     //Parameter value type, see CFG_PARAM_TYPE_t
    const char *dstring;       //Detailed description of the parameter
    uint32_t repeatdelay;      //Nonzero if continuous tap of value should be detected. Number of ms to sleep between callbacks
    uint32_t (*isvalid)(void); //Optional callback that can be defined. This function should return zero if parameter should not be displayed.
    uint32_t resetRequired;    //Nonzero if reset is required to apply parameter
} CFG_CHANGEABLE_PARAM_DESCR_t;

//Integer array macro
#define CFG_IARR(...) \
    (const int32_t[]) { __VA_ARGS__ }
//Character array macro
#define CFG_SARR(...) \
    (const char *[]) { __VA_ARGS__ }
//Float array macro
#define CFG_FARR(...) \
    (const float[]) { __VA_ARGS__ }

/*
//Callback that returns nonzero if Si5351 frequency synthesizer is selected
static uint32_t isSi5351(void)
{
    return (uint32_t)(CFG_SYNTH_SI5351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE));
}

//Callback that returns nonzero if ADF4350 frequency synthesizer is selected
static uint32_t isADF4350(void)
{
    return (uint32_t)(CFG_SYNTH_ADF4350 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE));
}

//Callback that returns nonzero if ADF4351 frequency synthesizer is selected
static uint32_t isADF4351(void)
{
    return (uint32_t)(CFG_SYNTH_ADF4351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE));
}
*/

static uint32_t isShowHidden(void)
{
    return 1 == CFG_GetParam(CFG_PARAM_SHOW_HIDDEN);
}

static uint32_t isShowHiddenSi(void)
{
    //return (1 == CFG_GetParam(CFG_PARAM_SHOW_HIDDEN)) && (CFG_SYNTH_SI5351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE));
    return (1 == CFG_GetParam(CFG_PARAM_SHOW_HIDDEN));
}
//Array of user changeable parameters descriptors
static const CFG_CHANGEABLE_PARAM_DESCR_t cfg_ch_descr_table[] =
    {
        {.id = CFG_PARAM_OSL_SELECTED,
         .idstring = "OSL_SELECTED",
         .nvalues = 17,
         .values = CFG_IARR(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1),
         .strvalues = CFG_SARR("A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "None"),
         .type = CFG_PARAM_T_S32,
         .dstring = "Selected OSL file"},

        /*
    {
        .id = CFG_PARAM_SYNTH_TYPE,
        .idstring = "SYNTH_TYPE",
        .nvalues = 4,
        .values = CFG_IARR(CFG_SYNTH_SI5351, CFG_SYNTH_ADF4350, CFG_SYNTH_ADF4351, CFG_SYNTH_SI5338A),
        .strvalues = CFG_SARR("Si5351A", "2x ADF4350", "2x ADF4351", "Si5338A"),
        .type = CFG_PARAM_T_U32,
        .dstring = "Frequency synthesizer type used.",
        .isvalid = isShowHidden,
        .resetRequired = 1,
    },
*/
        {
            .id = CFG_PARAM_R0,
            .idstring = "Z0",
            .nvalues = 6,
            .values = CFG_IARR(28, 50, 75, 100, 150, 300),
            .type = CFG_PARAM_T_U32,
            .dstring = "Selected base impedance (Z0) for Smith Chart and VSWR"},
        {
            .id = CFG_PARAM_SI5351_XTAL_FREQ,
            .idstring = "SI5351_XTAL_FREQ",
            .nvalues = 6,
            .values = CFG_IARR(25000000ul, 26000000ul, 27000000ul, 30000000ul, 32000000ul, 33000000ul),
            .type = CFG_PARAM_T_U32,
            .dstring = "Si5351 XTAL frequency, Hz",
            .isvalid = isShowHiddenSi,
        },
        {
            .id = CFG_PARAM_SI5351_BUS_BASE_ADDR,
            .idstring = "SI5351_BUS_BASE_ADDR",
            .nvalues = 4,
            .values = CFG_IARR(0x00, 0xC0, 0xC4, 0xCE),
            .strvalues = CFG_SARR("Autodetect", "C0h", "C4h", "CEh"),
            .type = CFG_PARAM_T_U8,
            .dstring = "Si5351 i2c bus base address (default C0h)",
            .isvalid = isShowHiddenSi,
        },
        {
            .id = CFG_PARAM_SI5351_CORR,
            .idstring = "SI5351_CORR",
            .type = CFG_PARAM_T_S16,
            .dstring = "Si5351 XTAL frequency correction, Hz",
            .isvalid = isShowHiddenSi,
            .repeatdelay = 2, // Modifed for more speed adjust
        },
        {
            .id = CFG_PARAM_SI5351_MAX_FREQ,
            .idstring = "SI5351_MAX_FREQ",
            .type = CFG_PARAM_T_U32,
            .nvalues = 6, // was 2
            .strvalues = CFG_SARR("160 MHz", "200 MHz", "260 MHz", "270 MHz", "280 MHz", "290 MHz"),
            .values = CFG_IARR(160000000ul, 200000000ul, 260000000ul, 270000000ul, 280000000ul, 290000000ul),
            .dstring = "Maximum frequency that Si5351 can output (160/200/270/280/290 MHz)",
            .isvalid = isShowHiddenSi,
        },
        {.id = CFG_PARAM_SI5351_CAPS,
         .idstring = "SI5351_CAPS",
         .type = CFG_PARAM_T_U8,
         .nvalues = 3,
         .strvalues = CFG_SARR("6 pF", "8 pF", "10 pF"),
         .values = CFG_IARR(1, 2, 3),
         .dstring = "Crystal Internal Load Capacitance. Recalibrate F if changed.",
         .isvalid = isShowHiddenSi,
         .resetRequired = 1},
        {.id = CFG_PARAM_OSL_RLOAD,
         .idstring = "OSL_RLOAD",
         .nvalues = 4,
         .values = CFG_IARR(50, 75, 100, 150),
         .type = CFG_PARAM_T_U32,
         .dstring = "LOAD R for OSL calibration, Ohm"},
        {.id = CFG_PARAM_OSL_RSHORT,
         .idstring = "OSL_RSHORT",
         .nvalues = 3,
         .values = CFG_IARR(0, 5, 10),
         .type = CFG_PARAM_T_U32,
         .dstring = "SHORT R for OSL calibration, Ohm"},
        {.id = CFG_PARAM_OSL_ROPEN,
         .idstring = "OSL_ROPEN",
         .nvalues = 6,
         .values = CFG_IARR(300, 333, 500, 750, 1000, 999999),
         .strvalues = CFG_SARR("300", "333", "500", "750", "1000", "Open"),
         .type = CFG_PARAM_T_U32,
         .dstring = "OPEN R for OSL calibration, Ohm"},
        {.id = CFG_PARAM_OSL_NSCANS,
         .idstring = "OSL_NSCANS",
         .nvalues = 7,
         .values = CFG_IARR(1, 3, 5, 7, 9, 11, 15),
         .type = CFG_PARAM_T_U32,
         .dstring = "Number of scans to average during OSL calibration at each F"},
        {.id = CFG_PARAM_MEAS_NSCANS,
         .idstring = "MEAS_NSCANS",
         .nvalues = 7,
         .values = CFG_IARR(1, 3, 5, 7, 9, 11, 15),
         .type = CFG_PARAM_T_U32,
         .dstring = "Number of scans to average in measurement window"},
        {.id = CFG_PARAM_PAN_NSCANS,
         .idstring = "PAN_NSCANS",
         .nvalues = 7,
         .values = CFG_IARR(1, 3, 5, 7, 9, 11, 15),
         .type = CFG_PARAM_T_U32,
         .dstring = "Number of scans to average in panoramic window"},

        {.id = CFG_PARAM_PAN_AUTOSPEED,
         .idstring = "PAN AUTO SPEED",
         .nvalues = 7,
         .values = CFG_IARR(4, 8, 10, 12, 14, 16, 20),
         .type = CFG_PARAM_T_U32,
         .dstring = "Automatic measuring speed in panoramic"},
        {.id = CFG_PARAM_S21_AUTOSPEED,
         .idstring = "S21 AUTO SPEED",
         .nvalues = 7,
         .values = CFG_IARR(4, 8, 10, 12, 14, 16, 20),
         .type = CFG_PARAM_T_U32,
         .dstring = "Automatic measuring speed in VNA |S21| Gain"},
        {.id = CFG_PARAM_LIN_ATTENUATION,
         .idstring = "LIN_ATTENUATION",
         .nvalues = 11,
         .values = CFG_IARR(0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30),
         .type = CFG_PARAM_T_U8,
         .dstring = "Linear audio inputs attenuation, dB. Requires reset.",
         .isvalid = isShowHidden,
         .resetRequired = 1},
        {.id = CFG_PARAM_BRIDGE_RM,
         .idstring = "BRIDGE_RM",
         .nvalues = 4,
         .values = (int32_t *)CFG_FARR(1.f, 2.f, 5.1f, 10.f),
         .type = CFG_PARAM_T_F32,
         .dstring = "Bridge Rm value, Ohm",
         .isvalid = isShowHidden},
        {.id = CFG_PARAM_BRIDGE_RADD,
         .idstring = "BRIDGE_RADD",
         .nvalues = 7,
         .values = (int32_t *)CFG_FARR(33.f, 51.f, 75.f, 100.f, 120.f, 150.f, 200.f),
         .type = CFG_PARAM_T_F32,
         .dstring = "Bridge Radd value, Ohm",
         .isvalid = isShowHidden},
        {.id = CFG_PARAM_PAN_CENTER_F,
         .idstring = "PAN_CENTER_F",
         .nvalues = 2,
         .values = CFG_IARR(0, 1),
         .strvalues = CFG_SARR("Start F", "Center F"),
         .type = CFG_PARAM_T_U32,
         .dstring = "Select setting start or center F in panoramic window."},
        {.id = CFG_PARAM_COM_PORT,
         .idstring = "COM_PORT",
         .nvalues = 2,
         .values = CFG_IARR(COM1, COM2),
         .strvalues = CFG_SARR("COM1 (ST-Link)", "COM2 (on the shield)"),
         .type = CFG_PARAM_T_U32,
         .dstring = "Select serial port to be used for remote control. Requires reset.",
         .isvalid = isShowHidden,
         .resetRequired = 1},
        {.id = CFG_PARAM_COM_SPEED,
         .idstring = "COM_SPEED",
         .nvalues = 5,
         .values = CFG_IARR(9600, 19200, 38400, 57600, 115200),
         .type = CFG_PARAM_T_U32,
         .dstring = "Serial port baudrate. Requires reset.",
         .isvalid = isShowHidden,
         .resetRequired = 1},
        {.id = CFG_PARAM_LOWPWR_TIME,
         .idstring = "LOW POWER TIMER",
         .nvalues = 6,
         .values = CFG_IARR(0, 30000, 60000, 120000, 180000, 300000),
         .strvalues = CFG_SARR("Off", "30s", "1 min", "2 min", "3 min", "5 min"),
         .type = CFG_PARAM_T_U32,
         .dstring = "Enter low power mode (display off) after this period of inactivity. Tap to wake up."},
        {
            .id = CFG_PARAM_S11_SHOW,
            .idstring = "S11_GRAPH_SHOW",
            .type = CFG_PARAM_T_U32,
            .nvalues = 2,
            .values = CFG_IARR(0, 1),
            .strvalues = CFG_SARR("No", "Yes"),
            .dstring = "Set to Yes to show S11 graph in the panoramic window",
        },
        {
            .id = CFG_PARAM_S1P_TYPE,
            .idstring = "S1P FILE TYPE",
            .type = CFG_PARAM_T_U32,
            .nvalues = 3,
            .values = CFG_IARR(CFG_S1P_TYPE_S_MA, CFG_S1P_TYPE_S_RI, CFG_S1P_TYPE_Z_RI),
            .strvalues = CFG_SARR("S MA R 50", "S RI R 50", "Z RI R50"),
            .dstring = "Touchstone S1P file type saved with screenshot. Default is S MA R 50.",
        },
        {.id = CFG_PARAM_BAND_FMIN,
         .idstring = "BAND_FMIN",
         .type = CFG_PARAM_T_U32,
         .nvalues = 5,                                                                     // ** WK **
         .values = CFG_IARR(100000ul, 200000ul, 300000ul, 400000ul, 500000ul),             // ** WK **
         .strvalues = CFG_SARR("100 kHz", "200 kHz*", "300 kHz*", "400 kHz*", "500 kHz*"), // ** WK **
         .isvalid = isShowHidden,
         .dstring = "Lower frequency band limit. If changed, full recalibration is required.",
         .resetRequired = 1},
        {.id = CFG_PARAM_BAND_FMAX,
         .idstring = "BAND_FMAX",
         .type = CFG_PARAM_T_U32,
         .nvalues = 16, // ** WK **
         .values = CFG_IARR(150000000ul, 200000000ul, 300000000ul, 450000000ul, 480000000ul, 500000000ul, 550000000ul,
                            590000000ul, 600000000ul, 810000000ul, 870000000ul, 1000000000ul, 1300000000ul, 1350000000ul, 1400000000ul, 1450000000ul), // ** WK **
         .strvalues = CFG_SARR("150 MHz", "200 MHz*", "300 MHz*", "450 MHz*", "480 MHz*", "500 MHz*", "550 MHz*",
                               "590 MHz*", "600 MHz*", "810 MHz*", "870 MHz*", "1000 MHz*", "1300 MHz*", "1350 MHz*", "1400 MHz*", "1450 MHz*"), // ** WK **
         .isvalid = isShowHidden,
         .dstring = "Upper frequency band limit. If changed, full recalibration is required.",
         .resetRequired = 1},
        {
            .id = CFG_PARAM_SCREENSHOT_FORMAT,
            .idstring = "SCREENSHOT_FORMAT",
            .type = CFG_PARAM_T_U32,
            .nvalues = 2,
            .values = CFG_IARR(0, 1),
            .strvalues = CFG_SARR("BMP", "PNG"),
            .isvalid = isShowHidden,
            .dstring = "Screenshot file format",
        },
        {
            .id = CFG_PARAM_TDR_VF,
            .idstring = "TDR Vf",
            .dstring = "Velocity factor for TDR, percent (1..100)",
            .type = CFG_PARAM_T_U8,
            .repeatdelay = 100,
        },
        {
            .id = CFG_PARAM_SHOW_HIDDEN,
            .idstring = "SHOW_HIDDEN",
            .type = CFG_PARAM_T_U32,
            .nvalues = 2,
            .values = CFG_IARR(0, 1),
            .strvalues = CFG_SARR("No", "Yes"),
            .dstring = "Show hidden menu parameters.",
        },
        {
            //additions 19.09.2020 DH1AKF (DG2DRF, VE7IT)
            .id = CFG_PARAM_SEREMUL, // added by VE7IT
            .idstring = "SERIAL_EMULATION",
            .type = CFG_PARAM_T_U32,
            .nvalues = 3,
            .values = CFG_IARR(CFG_PROTO_AA600, CFG_PROTO_LINSMITH, CFG_PROTO_NANOVNA),
            .strvalues = CFG_SARR("AA600", "N2PK Linsmith", "NanoVNA"),
            .dstring = "Serial remote protocol emulation",
        },
        {.id = CFG_PARAM_BT_SPEED,
         .idstring = "BT_SPEED",
         .nvalues = 5,
         .values = CFG_IARR(9600, 19200, 38400, 57600, 115200),
         .type = CFG_PARAM_T_U32,
         .dstring = "Bluetooth baudrate. Requires reset.",
         .isvalid = isShowHidden,
         .resetRequired = 1},
        {
            .id = CFG_PARAM_REGION, // added by DG2DRF
            .idstring = "IARU Region  ",
            .type = CFG_PARAM_T_U32,
            .nvalues = 4,
            .values = CFG_IARR(0, 1, 2, 3),
            .strvalues = CFG_SARR("REGION 1", "REGION 2", "REGION 3", "SRD"), // Short Range Devices
            .dstring = "Select IARU Region",
        },
        {
            .id = CFG_PARAM_ORIENTATION,         // added by DH1AKF
            .idstring = "Screen 0/180 degrees ", // 29.09.2020
            .type = CFG_PARAM_T_U32,
            .nvalues = 2,
            .values = CFG_IARR(0, 1),
            .strvalues = CFG_SARR("0 degrees", "180 degrees"),
            .dstring = "Screen Orientation",
        },
        {
            .id = CFG_PARAM_LOGLOG,         // added by DH1AKF
            .idstring = "SWR Log / LogLog", // 05.10.2020
            .type = CFG_PARAM_T_U32,
            .nvalues = 2,
            .values = CFG_IARR(0, 1),
            .strvalues = CFG_SARR("Logarithmic", "Double Log"),
            .dstring = "SWR scale",
        },
        {
            .id = CFG_PARAM_CURSOR,  // added by DH1AKF
            .idstring = "Cursor",    // 05.10.2020
            .type = CFG_PARAM_T_U32, // extended 03.11.2020 (Strict Auto Cursor)
            .nvalues = 3,
            .values = CFG_IARR(0, 1, 2),
            .strvalues = CFG_SARR("No Auto Cursor", "Auto Cursor", "Strict Auto Cursor"),
            .dstring = "AutoCursor behavior",
        },
        {
            .id = CFG_PARAM_ATTENUATOR,       // added by DH1AKF
            .idstring = "Attenuator for S21", // 03.11.2020 / 19.11.2020
            .type = CFG_PARAM_T_U32,
            .nvalues = 7,
            .values = CFG_IARR(10, 16, 20, 30, 40, 50, 60),
            .strvalues = CFG_SARR("10 dB", "16 dB", "20 dB", "30 dB", "40 dB", "50 dB", "60 dB"),
            .dstring = "Calibrate S21 with additional Attenuator",
        },
        {
            .id = CFG_PARAM_ShowLogoTime, // added by DH1AKF
            .idstring = "Show Logo Time", // 16.11.2020
            .type = CFG_PARAM_T_U32,
            .nvalues = 7,
            .values = CFG_IARR(0, 5, 10, 20, 30, 60, 3600),
            .strvalues = CFG_SARR("0 s", "5 s", "10 s", "20 s", "30 s", "1 min", "1 hour"),
            .dstring = "Choose the time for logo view",
        },
};

static const uint32_t cfg_ch_descr_table_num = sizeof(cfg_ch_descr_table) / sizeof(CFG_CHANGEABLE_PARAM_DESCR_t);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
//CFG module initialization
void CFG_Init(void)
{
    //Set defaults for all parameters
    CFG_SetParam(CFG_PARAM_VERSION, *(uint32_t *)AAVERSION);
    CFG_SetParam(CFG_PARAM_PAN_F1, 14000000ul);
    CFG_SetParam(CFG_PARAM_PAN_SPAN, 1ul);
    CFG_SetParam(CFG_PARAM_MEAS_F, 14000000ul);
    CFG_SetParam(CFG_PARAM_SYNTH_TYPE, 0);
    CFG_SetParam(CFG_PARAM_SI5351_XTAL_FREQ, 27000000ul);
    CFG_SetParam(CFG_PARAM_SI5351_BUS_BASE_ADDR, 0xC0); // wk 22.01.2019  (Changed Default value)
    CFG_SetParam(CFG_PARAM_SI5351_CORR, 0);
    CFG_SetParam(CFG_PARAM_OSL_SELECTED, 0ul); // wk 21.01.2019
    CFG_SetParam(CFG_PARAM_R0, 50);
    CFG_SetParam(CFG_PARAM_OSL_RLOAD, 50);
    CFG_SetParam(CFG_PARAM_OSL_RSHORT, 5);
    CFG_SetParam(CFG_PARAM_OSL_ROPEN, 500);
    CFG_SetParam(CFG_PARAM_OSL_NSCANS, 1);
    CFG_SetParam(CFG_PARAM_MEAS_NSCANS, 1);
    CFG_SetParam(CFG_PARAM_PAN_NSCANS, 1);
    CFG_SetParam(CFG_PARAM_LIN_ATTENUATION, 6);
    CFG_SetParam(CFG_PARAM_F_LO_DIV_BY_TWO, 0);
    CFG_SetParam(CFG_PARAM_GEN_F, 14000000ul);
    CFG_SetParam(CFG_PARAM_PAN_CENTER_F, 0);
    float tmp = 5.1f;
    CFG_SetParam(CFG_PARAM_BRIDGE_RM, *((uint32_t *)&tmp));
    tmp = 200.f;
    CFG_SetParam(CFG_PARAM_BRIDGE_RADD, *((uint32_t *)&tmp));
    tmp = 51.f;
    CFG_SetParam(CFG_PARAM_BRIDGE_RLOAD, *((uint32_t *)&tmp));
    CFG_SetParam(CFG_PARAM_COM_PORT, COM1);
    CFG_SetParam(CFG_PARAM_COM_SPEED, 38400);
    CFG_SetParam(CFG_PARAM_LOWPWR_TIME, 0);
    CFG_SetParam(CFG_PARAM_3RD_HARMONIC_ENABLED, 0);
    CFG_SetParam(CFG_PARAM_S11_SHOW, 1);
    CFG_SetParam(CFG_PARAM_S1P_TYPE, 0);
    CFG_SetParam(CFG_PARAM_SHOW_HIDDEN, 0);
    CFG_SetParam(CFG_PARAM_SCREENSHOT_FORMAT, 0);

    CFG_SetParam(CFG_PARAM_BAND_FMIN, 100000ul);
    CFG_SetParam(CFG_PARAM_BAND_FMAX, 600000000ul);
    CFG_SetParam(CFG_PARAM_SI5351_MAX_FREQ, 200000000ul);
    CFG_SetParam(CFG_PARAM_SI5351_CAPS, 3);
    CFG_SetParam(CFG_PARAM_TDR_VF, 66);
    CFG_SetParam(CFG_PARAM_Volt_max_Display, 4000);
    CFG_SetParam(CFG_PARAM_Volt_min_Display, 3100);

    CFG_SetParam(CFG_PARAM_Daylight, 0);    // Daylight (1)  Inhouse  (0)
    CFG_SetParam(CFG_PARAM_Fatlines, 0);    // Fat Lines (1) Thin Lines (0)
    CFG_SetParam(CFG_PARAM_BeepOn, 1);      // Beep on (1) Beep off (0)
    CFG_SetParam(CFG_PARAM_Date, 20190322); // Date yyyymmdd
    CFG_SetParam(CFG_PARAM_Time, 1930);

    //Added by wk 2019
    CFG_SetParam(CFG_PARAM_MULTI_F1, 3600);
    CFG_SetParam(CFG_PARAM_MULTI_BW1, 100000);

    //Added by KD8CEC
    CFG_SetParam(CFG_PARAM_LC_OSL_SELECTED, 0); //~0ul);    //OSL file for LC meter
    CFG_SetParam(CFG_PARAM_LC_INDEX, 3);        //~0ul);      //3~5Mhz

    CFG_SetParam(CFG_PARAM_S21_F1, 14000000ul);
    CFG_SetParam(CFG_PARAM_S21_SPAN, 1ul);

    //Antenna Select
    CFG_SetParam(CFG_PARAM_MULTI_ANT, 0); //Default Antenna Index is 0

    CFG_SetParam(CFG_PARAM_MULTI_2F1, 14000);
    CFG_SetParam(CFG_PARAM_MULTI_2BW1, 100000);

    CFG_SetParam(CFG_PARAM_MULTI_3F1, 14000);
    CFG_SetParam(CFG_PARAM_MULTI_3BW1, 100000);

    CFG_SetParam(CFG_PARAM_MULTI_4F1, 14000);
    CFG_SetParam(CFG_PARAM_MULTI_4BW1, 100000);

    CFG_SetParam(CFG_PARAM_MULTI_5F1, 14000);
    CFG_SetParam(CFG_PARAM_MULTI_5BW1, 100000);

    CFG_SetParam(CFG_PARAM_PAN_AUTOSPEED, 8);
    CFG_SetParam(CFG_PARAM_S21_AUTOSPEED, 8);

    CFG_SetParam(CFG_PARAM_SEREMUL, 0);     // VE7IT default serial emulation is AA600
    CFG_SetParam(CFG_PARAM_BT_SPEED, 9600); // DG2DRF default Bluetooth adapter speed
    CFG_SetParam(CFG_PARAM_REGION, 0);      // default: Region 1
    CFG_SetParam(CFG_PARAM_ORIENTATION, 0); // DH1AKF Screen orientation 29.09.2020
    CFG_SetParam(CFG_PARAM_LOGLOG, 1);
    CFG_SetParam(CFG_PARAM_CURSOR, 1);
    CFG_SetParam(CFG_PARAM_ATTENUATOR, 40);
    CFG_SetParam(CFG_PARAM_ShowLogoTime, 10);
    //Load parameters from file to the SD card
    FRESULT res;
    FIL fo = {0};

    FILINFO finfo;
    res = f_stat(g_cfg_fpath, &finfo);
    if (FR_NOT_ENABLED == res || FR_NOT_READY == res)
        CRASH("No SD card");
    if (FR_OK == res)
    {
        //Configuration file has been found on the SD card
        if (0 == finfo.fsize)
        {
            //Configuration file is empty - flush default configuration to it
            CFG_Flush();
        }
        //Open configuration file for reading
        res = f_open(&fo, g_cfg_fpath, FA_READ);
        if (finfo.fsize < sizeof(g_cfg_array))
        {
            //The configuration file on the SD card is smaller than configuration structure
            //Read partially
            UINT br;
            f_read(&fo, g_cfg_array, finfo.fsize, &br);
            f_close(&fo);
            //Replace configuration version to current
            CFG_SetParam(CFG_PARAM_VERSION, *(uint32_t *)AAVERSION);
            //And write extended file
            CFG_Flush();
        }
        else
        {
            //The configuration file on the SD card is equal or bigger in size than configuration structure
            //Read the whole structure
            UINT br;
            f_read(&fo, g_cfg_array, sizeof(g_cfg_array), &br);
            f_close(&fo);
            //Replace configuration version to current, but don't flush it
            CFG_SetParam(CFG_PARAM_VERSION, *(uint32_t *)AAVERSION);
        }
    }
    else
    {
        //Configuration file has not been found on the SD card
        res = f_mkdir(g_aa_dir);
        res = f_mkdir(g_cfg_dir);
        CFG_Flush();
        return;
    }
    //Verify critical parameters
    if (CFG_SYNTH_SI5351 == CFG_GetParam(CFG_PARAM_SYNTH_TYPE))
    {
        if (CFG_GetParam(CFG_PARAM_BAND_FMAX) > MAX_BAND_FREQ)
            CFG_SetParam(CFG_PARAM_BAND_FMAX, MAX_BAND_FREQ);
        if (CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ) != 160000000ul && CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ) != 200000000ul && CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ) != 260000000ul && CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ) != 270000000ul && CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ) != 280000000ul && CFG_GetParam(CFG_PARAM_SI5351_MAX_FREQ) != 290000000ul)
            CFG_SetParam(CFG_PARAM_SI5351_MAX_FREQ, 160000000ul);
    }

    if ((CFG_GetParam(CFG_PARAM_BAND_FMIN) >= 500000ul))
        CFG_SetParam(CFG_PARAM_BAND_FMIN, 500000ul);
    if ((CFG_GetParam(CFG_PARAM_BAND_FMAX) <= BAND_FMIN) || (CFG_GetParam(CFG_PARAM_BAND_FMAX) % 1000000 != 0))
        CFG_SetParam(CFG_PARAM_BAND_FMAX, 150000000ul);
    if (CFG_GetParam(CFG_PARAM_TDR_VF) < 1 || CFG_GetParam(CFG_PARAM_TDR_VF) > 100)
        CFG_SetParam(CFG_PARAM_TDR_VF, 66);

    if (CFG_GetParam(CFG_PARAM_PAN_AUTOSPEED) < 4 || CFG_GetParam(CFG_PARAM_PAN_AUTOSPEED) > 20)
        CFG_SetParam(CFG_PARAM_PAN_AUTOSPEED, 8);

    if (CFG_GetParam(CFG_PARAM_S21_AUTOSPEED) < 4 || CFG_GetParam(CFG_PARAM_S21_AUTOSPEED) > 20)
        CFG_SetParam(CFG_PARAM_S21_AUTOSPEED, 8);
}
#pragma GCC diagnostic pop

uint32_t CFG_GetParam(CFG_PARAM_t param)
{
    assert_param(param >= 0 && param < CFG_NUM_PARAMS);
    return g_cfg_array[param];
}

void CFG_SetParam(CFG_PARAM_t param, uint32_t value)
{
    assert_param(param >= 0 && param < CFG_NUM_PARAMS);
    g_cfg_array[param] = value;
}

void CFG_Flush(void)
{
#ifdef _DEBUG_UART
    DBG_Str("Flush !!!");
#endif

    FRESULT res;
    FIL fo = {0};
    res = f_open(&fo, g_cfg_fpath, FA_OPEN_ALWAYS | FA_WRITE);
    if (FR_OK == res)
    {
        UINT bw;
        CFG_SetParam(CFG_PARAM_VERSION, *(uint32_t *)AAVERSION);
        res = f_write(&fo, g_cfg_array, sizeof(g_cfg_array), &bw);
        res = f_close(&fo);
    }
    else
    {
        CRASHF("CFG_Flush: Failed to open config file. Err = %d", res);
    }
}

static uint32_t CFG_GetNextValue(uint32_t param_idx, uint32_t param_value)
{
    if (param_idx >= cfg_ch_descr_table_num)
        return param_value + 1;

    const CFG_CHANGEABLE_PARAM_DESCR_t *pd = &cfg_ch_descr_table[param_idx];
    if (0 == pd->nvalues)
    {
        //If no default values specified:
        return param_value + 1;
    }

    uint32_t i;
    for (i = 0; i < pd->nvalues; i++)
    {
        if (param_value == pd->values[i])
        {
            //Value is among the defaults
            if (i == pd->nvalues - 1)
                return pd->values[0]; //Wrap around the last one
            return pd->values[i + 1];
        }
    }
    //Oops, if we get here then the value is not among default ones
    //Just return the last default value
    return pd->values[pd->nvalues - 1];
}

static uint32_t CFG_GetPrevValue(uint32_t param_idx, uint32_t param_value)
{
    if (param_idx >= cfg_ch_descr_table_num)
        return param_value - 1;

    const CFG_CHANGEABLE_PARAM_DESCR_t *pd = &cfg_ch_descr_table[param_idx];
    if (0 == pd->nvalues)
    {
        //If no default values specified:
        return param_value - 1;
    }

    uint32_t i;
    for (i = 0; i < pd->nvalues; i++)
    {
        if (param_value == pd->values[i])
        {
            //Value is among the defaults
            if (i == 0)
                return pd->values[pd->nvalues - 1]; //Wrap around 0
            return pd->values[i - 1];
        }
    }
    //Oops, if we get here then the value is not among default ones
    //Just return the first default value
    return pd->values[0];
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
//Get string representation of parameter
const char *CFG_GetStringValue(uint32_t param_idx)
{
    if (param_idx >= cfg_ch_descr_table_num)
        return "";

    const CFG_CHANGEABLE_PARAM_DESCR_t *pd = &cfg_ch_descr_table[param_idx];

    uint32_t uval = CFG_GetParam(pd->id); //Current parameter value
    uint32_t i;

    if (0 != pd->strvalues)
    {
        for (i = 0; i < pd->nvalues; i++)
        {
            if (uval == pd->values[i])
                return pd->strvalues[i]; //Found string for default value
        }
    }

    //Print value to the static buffer and return it
    static char tstr[32];

    switch (pd->type)
    {
    case CFG_PARAM_T_U8:
        sprintf(tstr, "%u (%02Xh)", (unsigned int)((uint8_t)uval), (unsigned int)((uint8_t)uval));
        break;
    case CFG_PARAM_T_U16:
        sprintf(tstr, "%u", (unsigned int)((uint16_t)uval));
        break;
    case CFG_PARAM_T_U32:
        sprintf(tstr, "%u", (unsigned int)((uint32_t)uval));
        break;
    case CFG_PARAM_T_S8:
        sprintf(tstr, "%d", (int)((int8_t)uval));
        break;
    case CFG_PARAM_T_S16:
        sprintf(tstr, "%d", (int)((int16_t)uval));
        break;
    case CFG_PARAM_T_S32:
        sprintf(tstr, "%d", (int)((int32_t)uval));
        break;
    case CFG_PARAM_T_F32:
        sprintf(tstr, "%f", *(float *)&uval);
        break;
    case CFG_PARAM_T_CH:
        memcpy(tstr, &uval, 4);
        tstr[5] = '\0';
        break;
    default:
        return "";
    }
    return tstr;
}
#pragma GCC diagnostic pop

const char *CFG_GetStringDescr(uint32_t param_idx)
{
    if (param_idx >= cfg_ch_descr_table_num)
        return "";
    return cfg_ch_descr_table[param_idx].dstring;
}

const char *CFG_GetStringName(uint32_t param_idx)
{
    if (param_idx >= cfg_ch_descr_table_num)
        return "";
    return cfg_ch_descr_table[param_idx].idstring;
}

//===============================================================================
// Configuration parameters setting window
//===============================================================================
#include "LCD.h"
#include "touch.h"
#include "font.h"
#include "textbox.h"
extern void Sleep(uint32_t);

static uint32_t selected_param = 0;
static TEXTBOX_CTX_t *pctx = 0;
static uint32_t hbNameIdx = 0;
static uint32_t hbDescrIdx = 0;
static uint32_t hbValIdx = 0;
static uint32_t hbPrevValueIdx = 0;
static uint32_t hbNextValueIdx = 0;

//Added by KD8CEC, Version 0.35
static uint32_t hbPrevValueIdxBig = 0;
static uint32_t hbNextValueIdxBig = 0;

void SetConfigButtonsText()
{
    //added by KD8CEC
    if (cfg_ch_descr_table[selected_param].id == CFG_PARAM_SI5351_CORR)
    {
        TEXTBOX_SetText(pctx, hbPrevValueIdxBig, " << * 200 ");
        TEXTBOX_SetText(pctx, hbNextValueIdxBig, " * 200 >> ");
    }
    else
    {
        TEXTBOX_SetText(pctx, hbPrevValueIdxBig, "");
        TEXTBOX_SetText(pctx, hbNextValueIdxBig, "");
    }

    TEXTBOX_SetText(pctx, hbNameIdx, CFG_GetStringName(selected_param));
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    TEXTBOX_SetText(pctx, hbDescrIdx, CFG_GetStringDescr(selected_param));
    TEXTBOX_Find(pctx, hbPrevValueIdx)->nowait = cfg_ch_descr_table[selected_param].repeatdelay;
    TEXTBOX_Find(pctx, hbNextValueIdx)->nowait = cfg_ch_descr_table[selected_param].repeatdelay;
}

static void _hit_prev(void)
{
    for (;;)
    {
        if (--selected_param >= cfg_ch_descr_table_num)
            selected_param = cfg_ch_descr_table_num - 1;
        //Bypass changeable parameters for which isvalid() is defined and it returns zero
        if (cfg_ch_descr_table[selected_param].isvalid != 0)
        {
            if (cfg_ch_descr_table[selected_param].isvalid())
                break;
        }
        else
            break;
    }

    //Changed by KD8CEC
    SetConfigButtonsText();
    /*
    TEXTBOX_SetText(pctx, hbNameIdx, CFG_GetStringName(selected_param));
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    TEXTBOX_SetText(pctx, hbDescrIdx, CFG_GetStringDescr(selected_param));
    TEXTBOX_Find(pctx, hbPrevValueIdx)->nowait = cfg_ch_descr_table[selected_param].repeatdelay;
    TEXTBOX_Find(pctx, hbNextValueIdx)->nowait = cfg_ch_descr_table[selected_param].repeatdelay;
    */
}

static void _hit_next(void)
{
    for (;;)
    {
        selected_param++;
        if (selected_param >= cfg_ch_descr_table_num)
            selected_param = 0;
        //Bypass changeable parameters for which isvalid() is defined and it returns zero
        if (cfg_ch_descr_table[selected_param].isvalid != 0)
        {
            if (cfg_ch_descr_table[selected_param].isvalid())
                break;
        }
        else
            break;
    }

    SetConfigButtonsText();
    /*
    TEXTBOX_SetText(pctx, hbNameIdx, CFG_GetStringName(selected_param));
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    TEXTBOX_SetText(pctx, hbDescrIdx, CFG_GetStringDescr(selected_param));
    TEXTBOX_Find(pctx, hbPrevValueIdx)->nowait = cfg_ch_descr_table[selected_param].repeatdelay;
    TEXTBOX_Find(pctx, hbNextValueIdx)->nowait = cfg_ch_descr_table[selected_param].repeatdelay;
    */
}

static void _hit_save(void)
{
    CFG_Flush();
    GEN_Init(); //In case if synthesizer type has changed
    if (0 != resetRequired)
    {
        Sleep(200);
        NVIC_SystemReset();
    }
    rqExit = 1;
}

static void _hit_ex(void)
{
    rqExit = 1;
}

static void _hit_prev_value(void)
{
    uint32_t currentValue = CFG_GetParam(cfg_ch_descr_table[selected_param].id);
    uint32_t prevValue = CFG_GetPrevValue(selected_param, currentValue);
    CFG_SetParam(cfg_ch_descr_table[selected_param].id, prevValue);
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    resetRequired += cfg_ch_descr_table[selected_param].resetRequired;
}

static void _hit_next_value(void)
{
    uint32_t currentValue = CFG_GetParam(cfg_ch_descr_table[selected_param].id);
    uint32_t nextValue = CFG_GetNextValue(selected_param, currentValue);
    CFG_SetParam(cfg_ch_descr_table[selected_param].id, nextValue);
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    resetRequired += cfg_ch_descr_table[selected_param].resetRequired;
}

//added by KD8CEC
static void _hit_prev_valueBig(void)
{
    uint32_t currentValue = CFG_GetParam(cfg_ch_descr_table[selected_param].id);
    uint32_t prevValue = currentValue - 200;

    CFG_SetParam(cfg_ch_descr_table[selected_param].id, prevValue);
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    resetRequired += cfg_ch_descr_table[selected_param].resetRequired;
}

static void _hit_next_valueBig(void)
{
    uint32_t currentValue = CFG_GetParam(cfg_ch_descr_table[selected_param].id);
    uint32_t nextValue = currentValue + 200;
    CFG_SetParam(cfg_ch_descr_table[selected_param].id, nextValue);
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    resetRequired += cfg_ch_descr_table[selected_param].resetRequired;
}

extern uint8_t NotSleepMode;

// Changeable parameters editor window
// See these parameters described in cfg_ch_descr_table
void CFG_ParamWnd(void)
{
    while (TOUCH_IsPressed())
        ;
    rqExit = 0;
    resetRequired = 0;
    selected_param = 0;

    LCD_FillAll(LCD_BLACK);

    FONT_Write(FONT_FRANBIG, LCD_RED, LCD_BLACK, 100, 0, "Configuration editor");

    TEXTBOX_t hbPrevParam = {.x0 = 10, .y0 = 34, .text = " < Prev param ", .font = FONT_FRANBIG, .fgcolor = LCD_BLACK, .bgcolor = LCD_RGB(128, 128, 128), .cb = _hit_prev};
    TEXTBOX_t hbNextParam = {.x0 = 300, .y0 = 34, .text = " Next param > ", .font = FONT_FRANBIG, .fgcolor = LCD_BLACK, .bgcolor = LCD_RGB(128, 128, 128), .cb = _hit_next};
    TEXTBOX_t hbEx = {.x0 = 10, .y0 = 220, .text = " Cancel and exit ", .font = FONT_FRANBIG, .fgcolor = LCD_BLUE, .bgcolor = LCD_YELLOW, .cb = _hit_ex};
    TEXTBOX_t hbSave = {.x0 = 300, .y0 = 220, .text = " Save and exit ", .font = FONT_FRANBIG, .fgcolor = LCD_BLUE, .bgcolor = LCD_GREEN, .cb = _hit_save};

    TEXTBOX_t hbParamName = {.x0 = 10, .y0 = 70, .text = "    ", .font = FONT_FRANBIG, .fgcolor = LCD_GREEN, .bgcolor = LCD_BLACK, .cb = 0, .nowait = 1};
    TEXTBOX_t hbParamDescr = {.x0 = 10, .y0 = 110, .text = "    ", .font = FONT_FRAN, .fgcolor = LCD_WHITE, .bgcolor = LCD_BLACK, .cb = 0, .nowait = 1};
    TEXTBOX_t hbValue = {.x0 = 80, .y0 = 130, .text = "    ", .font = FONT_FRANBIG, .fgcolor = LCD_BLUE, .bgcolor = LCD_BLACK, .cb = 0, .nowait = 1};
    TEXTBOX_t hbPrevValue = {.x0 = 10, .y0 = 130, .text = "  <  ", .font = FONT_FRANBIG, .fgcolor = LCD_BLACK, .bgcolor = LCD_RGB(128, 128, 128), .cb = _hit_prev_value};
    TEXTBOX_t hbNextValue = {.x0 = 400, .y0 = 130, .text = "  >  ", .font = FONT_FRANBIG, .fgcolor = LCD_BLACK, .bgcolor = LCD_RGB(128, 128, 128), .cb = _hit_next_value};

    //Added by KD8CEC, Version 0.35
    TEXTBOX_t hbPrevValueBig = {.x0 = 10, .y0 = 170, .text = "", .font = FONT_FRANBIG, .fgcolor = LCD_YELLOW, .bgcolor = LCD_BLACK, .cb = _hit_prev_valueBig};
    TEXTBOX_t hbNextValueBig = {.x0 = 327, .y0 = 170, .text = "", .font = FONT_FRANBIG, .fgcolor = LCD_YELLOW, .bgcolor = LCD_BLACK, .cb = _hit_next_valueBig};

    TEXTBOX_CTX_t ctx = {0};
    pctx = &ctx;
    TEXTBOX_InitContext(pctx);

    TEXTBOX_Append(pctx, &hbPrevParam);
    TEXTBOX_Append(pctx, &hbNextParam);
    TEXTBOX_Append(pctx, &hbEx);
    TEXTBOX_Append(pctx, &hbSave);
    hbPrevValueIdx = TEXTBOX_Append(pctx, &hbPrevValue);
    hbNextValueIdx = TEXTBOX_Append(pctx, &hbNextValue);

    //Added by KD8CEC, Version 0.35
    hbPrevValueIdxBig = TEXTBOX_Append(pctx, &hbPrevValueBig);
    hbNextValueIdxBig = TEXTBOX_Append(pctx, &hbNextValueBig);

    hbNameIdx = TEXTBOX_Append(pctx, &hbParamName);
    hbDescrIdx = TEXTBOX_Append(pctx, &hbParamDescr);
    hbValIdx = TEXTBOX_Append(pctx, &hbValue);
    TEXTBOX_DrawContext(&ctx);

    TEXTBOX_SetText(pctx, hbNameIdx, CFG_GetStringName(selected_param));
    TEXTBOX_SetText(pctx, hbValIdx, CFG_GetStringValue(selected_param));
    TEXTBOX_SetText(pctx, hbDescrIdx, CFG_GetStringDescr(selected_param));

    NotSleepMode = 1;
    for (;;)
    {
        if (TEXTBOX_HitTest(&ctx))
        {
            if (rqExit)
            {
                CFG_Init();
                break;
            }
            Sleep(50);
        }
        Sleep(0);
    }
    NotSleepMode = 0;
}
