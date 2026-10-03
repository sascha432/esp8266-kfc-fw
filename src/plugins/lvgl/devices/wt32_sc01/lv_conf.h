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

/* Swap the 2 bytes of RGB565. LovyanGFX' setSwapBytes() is false by default, so the
   panel receives the low byte of every pixel first and the LVGL buffer has to hold
   byte swapped pixels. This is the combination the official LovyanGFX LVGL example
   uses (examples/Advanced/LVGL_PlatformIO: LV_COLOR_DEPTH 16 + LV_COLOR_16_SWAP 1).
   With 0 the 16 bit pixel is scrambled - the green bar shows up red and the blue
   touch pad shows up green. Never combine with lcd.setSwapBytes(true), exactly one
   of the two may be active. */
#define LV_COLOR_16_SWAP 1

/*------------------
 * MEMORY SETTINGS
 *------------------*/

/* LVGL's own allocator. The pool and the large buffers (draw buffers, canvas, images) come
   from PSRAM (8 MB on the WROVER-B): the ~180 KB of internal DRAM are shared with the WiFi
   stack, the AsyncWebServer and the panel driver, a static pool in DRAM does not fit there
   anymore (it overflowed dram0_0_seg as soon as the OpenWeatherMap TLS client was added).
   Only the pool is moved, LVGL keeps its own TLSF allocator. The pool size is down to 32 KB
   because the peak usage of this UI is around 23 KB (a 64 KB pool only wasted PSRAM). */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (32U * 1024U) /* [bytes] */
#define LV_MEM_POOL_INCLUDE "lvgl_psram_pool.h"
#define LV_MEM_POOL_ALLOC lvgl_psram_pool_alloc

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
#ifndef LV_DISP_DEF_REFR_PERIOD
#define LV_DISP_DEF_REFR_PERIOD 30  /* [ms] */
#define LV_INDEV_DEF_READ_PERIOD LV_DISP_DEF_REFR_PERIOD /* [ms] */
#endif

/*------------------
 * RENDERING / ANTI ALIASING
 *------------------*/

/* Complex draw engine: shadows, gradients, rounded corners, circles, arcs, skew
   lines, image transformations, masks - and the anti-aliasing of all of them.
   LVGL's default is already 1, it is written out here so the setting is visible. */
#define LV_DRAW_COMPLEX 1

/* Cache of the 1/4 circle outlines used to anti-alias rounded corners (radius * 4 bytes each) */
#define LV_CIRCLE_CACHE_SIZE 4

/* Shadow buffer cache. This UI uses no shadows, caching would only cost RAM. */
#define LV_SHADOW_CACHE_SIZE 0

/* Dithering of gradients, only affects gradients on 16bpp and costs RAM */
#define LV_DITHER_GRADIENT 0

/* Subpixel (RGB) text rendering. The built in Montserrat fonts are converted as
   4bpp grayscale + anti-aliasing, they contain no subpixel data, so this cannot be
   used with them. It also requires a font converted with subpixel support. */
#define LV_USE_FONT_SUBPX 0

/* Decompression of RLE compressed fonts (Font Converter "Compress" / lv_font_conv
   without --no-compress). The MDI icon fonts in src/plugins/lvgl/fonts/ are compressed
   (bitmap_format = LV_FONT_FMT_TXT_COMPRESSED), without this flag their glyphs are not
   decoded and every icon stays blank. The built in Montserrat fonts are uncompressed and
   do not need it. */
#define LV_USE_FONT_COMPRESSED 1

/*------------------
 * WIDGETS
 *------------------*/

/* Pinned so the UI does not depend on the LVGL defaults. The application screens use
   labels, plain objects (cards, icons), lines (icon details), the flex layout and the chart
   (power monitor); the widgets for the later screens (meter, msgbox, snapshot) are off until
   they are implemented - each of them costs flash. */
#define LV_USE_LABEL 1
#define LV_USE_IMG 1
#define LV_USE_LINE 1
#define LV_USE_BAR 1
#define LV_USE_ARC 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_USE_CHART 1
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_TABLE 0
#define LV_USE_CANVAS 0
#define LV_USE_ANIMIMG 0
#define LV_USE_SPINNER 0
/* Snapshot: renders an object (the active screen) into a buffer, used by the debug screenshot
   feature (DEBUG_LVGL_SCREENSHOT, src/plugins/lvgl/lvgl_debug.cpp). It reuses the draw engine,
   so the flash cost is small - the buffer is allocated by the application (PSRAM). */
#define LV_USE_SNAPSHOT 1
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_LOG 0

/*------------------
 * FONTS
 *------------------*/

/* the built in fonts are enough to start with, size them in the UI (see LVGLUI::kFontLadder).
   12/14/16/20/24/28/32 covers everything the screens use, the smaller sizes would only
   cost flash */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1

/* default font for every widget that does not set its own (was montserrat_14) */
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#endif /* LV_CONF_H */
#endif /* 1 */
