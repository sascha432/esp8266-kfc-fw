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

#include <lvgl.h>

#include "lgfx_wt32_sc01.h"

namespace WT32_SC01 {

//! panel access, initialized by begin()
LGFX_WT32_SC01 &lcd();

//! LVGL display and input device registered by begin(), nullptr before that
lv_disp_t *display();
lv_indev_t *input();

//! initializes the panel, the backlight, LVGL and registers display + touch
//! @return false if the panel, the PSRAM draw buffers or LVGL failed
bool begin();

//! runs the LVGL timer handler, call it from the main loop
void loop();

//! backlight, 0 = off, 255 = maximum
void setBacklight(uint8_t level);

} // namespace WT32_SC01
