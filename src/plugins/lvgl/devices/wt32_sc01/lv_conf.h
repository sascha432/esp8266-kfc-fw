/**
 * @file lv_conf.h
 *
 * Minimal LVGL 8.4 configuration for the Wireless-Tag WT32-SC01
 * (ESP32-WROVER-B, 3.5" 480x320 ST7796S, FT6336U capacitive touch).
 *
 * Everything that is not set here keeps the LVGL default from lv_conf_internal.h.
 * The complete list of options is in
 *   .pio/libdeps/<env>/lvgl/lv_conf_template.h
 * copy the settings you need from there into this file.
 *
 * Enabled with -D LV_CONF_INCLUDE_SIMPLE=1 and found through
 * -I./src/plugins/lvgl/devices/wt32_sc01 (see conf/envs/wt32_sc01.ini).
 */
#if 1

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*------------------
 * COLOR SETTINGS
 *------------------*/

/* 1: 1 byte per pixel, 8: RGB332, 16: RGB565, 32: ARGB8888 */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 - used with lcd.pushImage(), not together with
   lcd.setSwapBytes(true). If the colors look wrong, flip exactly one of the two. */
#define LV_COLOR_16_SWAP 0

/*------------------
 * MEMORY SETTINGS
 *------------------*/

/* LVGL's own allocator. The large buffers (draw buffers, canvas, images) are
   allocated by the application and should come from PSRAM (8MB on the WROVER-B). */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U) /* [bytes] */

/*------------------
 * TICK
 *------------------*/

/* use millis(), no lv_tick_inc() timer needed */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*------------------
 * DEFAULTS
 *------------------*/

#define LV_DPI_DEF 130              /* [px/inch] 3.5" @ 480x320 */
#define LV_DISP_DEF_REFR_PERIOD 30  /* [ms] */
#define LV_INDEV_DEF_READ_PERIOD 30 /* [ms] */

/*------------------
 * FONTS
 *------------------*/

/* the built in fonts are enough to start with, size them in the UI */
#define LV_FONT_MONTSERRAT_14 1

#endif /* LV_CONF_H */
#endif /* 1 */
