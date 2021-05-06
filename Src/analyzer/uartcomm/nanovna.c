#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "shell.h"
#include "build_timestamp.h"
#include "gen.h"
#include "config.h"
#include "dsp.h"
#include "oslfile.h"
#include "ctype.h"

//=======================================================================================
// Basic NanoVNA protocol emulation for NanoVNA Saver
// Parts of code has been taken from NanoVNA code
// Implementation is based on individual shell commands automatically added to the console
// with SHELL_CMD macro at compile time. See shell.c and shell.h for details.

#define START_MIN   BAND_FMIN
#define STOP_MAX    MAX_BAND_FREQ

#define frequency0 current_props._frequency0
#define frequency1 current_props._frequency1
#define sweep_points current_props._sweep_points
#define frequencies current_props._frequencies

enum {
  ST_START, ST_STOP, ST_CENTER, ST_SPAN, ST_CW
};

typedef struct {
  uint32_t _frequency0;
  uint32_t _frequency1;
  int16_t _sweep_points;
  uint32_t _frequencies[101];
} properties_t;

properties_t current_props __attribute__((section (".user_sdram")));
properties_t *active_props = &current_props;

static bool _is_initialized = false;
static float measured[2][101][2] __attribute__((section (".user_sdram")));

static void set_frequencies(uint32_t start, uint32_t stop, int16_t points);
static void update_frequencies(void);
static void freq_mode_startstop(void);
static void freq_mode_centerspan(void);
static void set_sweep_frequency(int type, uint32_t freq);

static void cmd_frequencies(int argc, char* const argv[]);
static void cmd_data(int argc, char* const argv[]);
static void cmd_sweep(int argc, char* const argv[]);

static void set_frequencies(uint32_t start, uint32_t stop, int16_t points)
{
  int i;
  float span = stop - start;
  for (i = 0; i < points; i++) {
    float offset = i * span / (float)(points - 1);
    frequencies[i] = start + (uint32_t)offset;
  }
  // disable at out of sweep range
  for (; i < 101; i++)
    frequencies[i] = 0;
}

static void update_frequencies(void)
{
  uint32_t start, stop;
  if (frequency0 < frequency1) {
    start = frequency0;
    stop = frequency1;
  } else {
    start = frequency1;
    stop = frequency0;
  }
  set_frequencies(start, stop, sweep_points);
}

static void freq_mode_startstop(void)
{
  if (frequency0 > frequency1) {
    uint32_t f = frequency1;
    frequency1 = frequency0;
    frequency0 = f;
  }
}

static void freq_mode_centerspan(void)
{
  if (frequency0 <= frequency1) {
    uint32_t f = frequency1;
    frequency1 = frequency0;
    frequency0 = f;
  }
}

static void set_sweep_frequency(int type, uint32_t freq)
{
  switch (type) {
  case ST_START:
    freq_mode_startstop();
    if (freq < START_MIN)
      freq = START_MIN;
    if (freq > STOP_MAX)
      freq = STOP_MAX;
    if (frequency0 != freq) {
      frequency0 = freq;
      // if start > stop then make start = stop
      if (frequency1 < freq)
        frequency1 = freq;
      update_frequencies();
    }
    break;
  case ST_STOP:
    freq_mode_startstop();
    if (freq > STOP_MAX)
      freq = STOP_MAX;
    if (freq < START_MIN)
      freq = START_MIN;
    if (frequency1 != freq) {
      frequency1 = freq;
      // if start > stop then make start = stop
      if (frequency0 > freq)
        frequency0 = freq;
      update_frequencies();
    }
    break;
  case ST_CENTER:
    if (freq < START_MIN)
      freq = START_MIN;
    if (freq > STOP_MAX)
      freq = STOP_MAX;
    freq_mode_centerspan();
    uint32_t center = frequency0/2 + frequency1/2;
    if (center != freq) {
      uint32_t span = frequency0 - frequency1;
      if (freq < START_MIN + span/2) {
        span = (freq - START_MIN) * 2;
      }
      if (freq > STOP_MAX - span/2) {
        span = (STOP_MAX - freq) * 2;
      }
      frequency0 = freq + span/2;
      frequency1 = freq - span/2;
      update_frequencies();
    }
    break;
  case ST_SPAN:
    if (freq > STOP_MAX)
      freq = STOP_MAX;
    freq_mode_centerspan();
    if (frequency0 - frequency1 != freq) {
      uint32_t center = frequency0/2 + frequency1/2;
      if (center < START_MIN + freq/2) {
        center = START_MIN + freq/2;
      }
      if (center > STOP_MAX - freq/2) {
        center = STOP_MAX - freq/2;
      }
      frequency1 = center - freq/2;
      frequency0 = center + freq/2;
      update_frequencies();
    }
    break;
  case ST_CW:
    if (freq < START_MIN)
      freq = START_MIN;
    if (freq > STOP_MAX)
      freq = STOP_MAX;
    freq_mode_centerspan();
    if (frequency0 != freq || frequency1 != freq) {
      frequency0 = freq;
      frequency1 = freq;
      update_frequencies();
    }
    break;
  }
}

static void cmd_sweep(int argc, char* const argv[])
{
  if (argc == 0) {
    printf("%lu %lu %d\r\n", frequency0, frequency1, sweep_points);
    return;
  } else if (argc > 3) {
    goto usage;
  }
  if (argc >= 2) {
    if (strcmp(argv[0], "start") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_START, value);
      return;
    } else if (strcmp(argv[0], "stop") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_STOP, value);
      return;
    } else if (strcmp(argv[0], "center") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_CENTER, value);
      return;
    } else if (strcmp(argv[0], "span") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_SPAN, value);
      return;
    } else if (strcmp(argv[0], "cw") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_CW, value);
      return;
    }
  }

  if (argc >= 1) {
    int32_t value = atoi(argv[0]);
    if (value == 0)
      goto usage;
    set_sweep_frequency(ST_START, value);
  }
  if (argc >= 2) {
    int32_t value = atoi(argv[1]);
    set_sweep_frequency(ST_STOP, value);
  }
  return;
usage:
  printf("usage: sweep {start(Hz)} [stop(Hz)]\r\n");
  printf("\tsweep {start|stop|center|span|cw} {freq(Hz)}\r\n");
}

static void cmd_frequencies(int argc, char* const argv[])
{
  int i;
  (void)argc;
  (void)argv;
  for (i = 0; i < sweep_points; i++)
  {
    if (frequencies[i] != 0)
    {
      printf("%lu\r\n", frequencies[i]);
    }
  }
}

static void cmd_data(int argc, char* const argv[])
{
  int i;
  int sel = 0;

  if (argc == 1)
    sel = atoi(argv[0]);
  if (sel == 0 || sel == 1) {
    for (i = 0; i < sweep_points; i++) {
      if (frequencies[i] != 0)
        printf("%f %f\r\n", measured[sel][i][0], measured[sel][i][1]);
    }
  } else if (sel >= 2 && sel < 7) {
    for (i = 0; i < sweep_points; i++) {
      if (frequencies[i] != 0)
      {
        printf("%f %f\r\n", 0.f, 0.f);
        //printf("%f %f\r\n", cal_data[sel-2][i][0], cal_data[sel-2][i][1]);
      }
    }
  } else {
    printf("usage: data [array]\r\n");
  }
}

//==============================================================================================
// Wrappers

static void _initialize(void)
{
    if (!_is_initialized)
    {
        //float measured[2][101][2]
        uint32_t i, j, k;
        for (i = 0; i < 2; i++)
        {
            for (j = 0; j < 101; j++)
            {
                for (k = 0; k < 2; k++)
                {
                    measured[i][j][k] = 0.0f;
                }
            }
        }

        memset(&current_props, 0, sizeof(current_props));
        current_props._frequency0 =       500000,
        current_props._frequency1 =       10500000,
        current_props._sweep_points =     101,
        set_frequencies(current_props._frequency0, current_props._frequency1, current_props._sweep_points);
        _is_initialized = true;
    }
}

static bool _nanovna_cmd_exit(uint32_t argc, char* const argv[])
{
    (void)argc; (void)argv;
    printf("ChibiOS/RT Shell\r\n"); // fake
    return false;
}
SHELL_CMD(exit, "exits and then restarts console mode", _nanovna_cmd_exit);

static bool _nanovna_cmd_info(uint32_t argc, char* const argv[])
{
    (void)argc; (void)argv;
    printf("Kernel:       none\r\n");
    printf("Compiler:     arm-none-eabi-gcc (GNU Tools for Arm Embedded Processors)\r\n");
    printf("Architecture: ARMv7-M\r\n");
    printf("Core Variant: Cortex-M7\r\n");
    printf("Port Info:    none\r\n");
    printf("Platform:     STM32F746G DISCO\r\n");
    printf("Board:        NanoVNA\r\n");
    printf("Build time:   %s%s%s\r\n",  __DATE__, " - ", __TIME__);
    return false;
}
SHELL_CMD(info, "prints firmware info on console", _nanovna_cmd_info);

static bool _nanovna_cmd_freq(uint32_t argc, char* const argv[])
{
    if (argc != 2)
    {
        printf("Usage: freq {frequency (Hz)}\r\n");
        return false;
    }
    uint32_t freq = 0;
    shell_str2num(argv[1], &freq);
    GEN_SetMeasurementFreq(freq);
    return false;
}
SHELL_CMD(freq, "set the CW frequency", _nanovna_cmd_freq);

// data
static bool _nanovna_cmd_data(uint32_t argc, char* const argv[])
{
    _initialize();
    cmd_data(argc - 1, &argv[1]);
    return false;
}
SHELL_CMD(data, "data [array]", _nanovna_cmd_data);

// sweep
static bool _nanovna_cmd_sweep(uint32_t argc, char* const argv[])
{
    _initialize();
    cmd_sweep(argc-1, &argv[1]);
    uint32_t i;
    for (i = 0; i < 101; i++)
    {
        if (frequencies[i] == 0)
        {
            break;
        }
        DSP_Measure(frequencies[i], 1, 1, 1);
        float complex g = OSL_GFromZ(DSP_MeasuredZ(), (float)CFG_GetParam(CFG_PARAM_R0));
        measured[0][i][0] = crealf(g);
        measured[0][i][1] = cimagf(g);
    }
    GEN_SetMeasurementFreq(0);
    return false;
}
SHELL_CMD(sweep, "sweep", _nanovna_cmd_sweep);

// frequencies
static bool _nanovna_cmd_frequencies(uint32_t argc, char* const argv[])
{
    _initialize();
    cmd_frequencies(argc-1, &argv[1]);
    return false;
}
SHELL_CMD(frequencies, "list frequencies", _nanovna_cmd_frequencies);
