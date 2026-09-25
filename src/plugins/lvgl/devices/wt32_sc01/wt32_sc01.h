/**
 * @file wt32_sc01.h
 *
 * WT32-SC01 3.5" touch display: LovyanGFX panel + LVGL 8 display and input driver.
 *
 * Usage (from the main loop / a task):
 *   WT32_SC01::begin();
 *   ...build the UI...
 *   // every iteration
 *   WT32_SC01::loop();
 */
#pragma once

#include <Arduino_compat.h>
#include <lvgl.h>

#include "lgfx_wt32_sc01.h"

// set to 1 for the device log output (-D DEBUG_WT32_SC01=1)
#ifndef DEBUG_WT32_SC01
#    define DEBUG_WT32_SC01 0
#endif

#if DEBUG_WT32_SC01
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WT32_SC01 {

//! panel access, initialized by begin()
LGFX_WT32_SC01 &lcd();

//! LVGL display and input device registered by begin(), nullptr before that
lv_disp_t *display();
lv_indev_t *input();

//! initializes the panel, the backlight, LVGL and registers display + touch
//! draw buffers are allocated from PSRAM and fall back to internal RAM
//! @return false if the panel does not respond or no draw buffer could be
//!         allocated, see lastError() for the reason
bool begin();

//! reason why begin() failed, nullptr after a successful call
const __FlashStringHelper *lastError();

//! runs the LVGL timer handler, call it from the main loop
void loop();

//! backlight, 0 = off, 255 = maximum. can be called before begin(), the level is
//! applied as soon as the panel is ready
void setBacklight(uint8_t level);

//! backlight level in use
uint8_t getBacklight();

} // namespace WT32_SC01
