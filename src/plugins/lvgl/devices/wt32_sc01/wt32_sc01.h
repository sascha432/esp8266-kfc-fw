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

// panel access, initialized by begin()
LGFX_WT32_SC01 &lcd();

// LVGL display and input device registered by begin(), nullptr before that
lv_disp_t *display();
lv_indev_t *input();

// initializes the panel, the backlight, LVGL and registers display + touch.
// Draw buffers are allocated from PSRAM and fall back to internal RAM.
// Returns false if the panel does not respond or no draw buffer could be
// allocated, see lastError() for the reason
bool begin();

// reason why begin() failed, nullptr after a successful call
const __FlashStringHelper *lastError();

// runs the LVGL timer handler, call it from the main loop
void loop();

// backlight, 0 = off, 255 = maximum. can be called before begin(), the level is
// applied as soon as the panel is ready
void setBacklight(uint8_t level);

// backlight level in use
uint8_t getBacklight();

// true if the touch controller answered on I2C during begin()
bool touchReady();

// true while the panel reports a touch
bool isTouched();

// number of touch presses the driver has seen, and the last raw touch point
// (false until the first touch, the values are for diagnostics only)
uint32_t getTouchCount();
bool getLastTouch(int32_t &x, int32_t &y);

// Enables/disables the input. While it is disabled the panel is still polled (isTouched() and
// getTouchCount() keep working) but the input is reported as released, so it is not passed to
// LVGL or the widgets. The power saving mode uses it to swallow the touch that wakes the
// display up, the first touch only turns the backlight on again
void setInputEnabled(bool enabled);
bool getInputEnabled();

// Orientation of the panel. The panel is portrait natively (320x480), LANDSCAPE is the orientation
// the display is used in by default. The touch transform is rotated by LovyanGFX together with the
// panel (no calibration is in use), so the input device needs no change. The LVGL resolution of the
// registered display is updated in place - the widget tree of the screen that is shown has to be
// rebuilt by its owner (the screen manager does that when a screen is created)
enum class Rotation : uint8_t {
    LANDSCAPE = 0,          // 480x320
    PORTRAIT = 1,           // 320x480
    LANDSCAPE_FLIPPED = 2,  // 480x320, turned 180 degrees
    PORTRAIT_FLIPPED = 3,   // 320x480, turned 180 degrees
};

// Switches the panel and the resolution of the LVGL display to another orientation. Returns false
// while the panel is not initialized. A call with the active rotation does nothing and returns true
bool setRotation(Rotation rotation);

// orientation in use, LANDSCAPE until setRotation() is called
Rotation getRotation();

// true for the two portrait rotations (the display resolution is 320x480)
bool isPortrait();

} // namespace WT32_SC01
