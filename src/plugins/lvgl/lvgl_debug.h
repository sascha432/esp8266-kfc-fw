/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Debug helper of the LVGL UI, enabled with -D DEBUG_LVGL_SCREENSHOT=1 (debug builds only):
//
//   GET /lvgl-screen              self contained debug page: screen selector, auto refresh, a
//                                 form to push data into the screens and the backlight control
//   GET /lvgl-screen.bmp          screenshot of the active screen as a 24 bit BMP
//
// Parameters of /lvgl-screen.bmp:
//   screen=<index|name>           shows that screen before the capture (MAIN, INDOOR, ...)
//   set=<key>:<value>[;...]       pushes values into the application before the capture, the
//                                 keys are application specific (see the help callback).
//                                 Example: set=temp:123.4;descr:a very long description;freeze:1
//   backlight=<0-100>             sets the backlight before the capture (useful for photos)
//
// The capture runs in the main loop (LVGL is not re-entrant): the HTTP handler prepares the
// request, waits for the main loop and then streams the BMP. Both buffers (the LVGL snapshot and
// the BMP) are allocated from PSRAM, ~307 KB + ~460 KB for the 480x320 panel.
//
// Purpose: check how a screen looks with specific values, mostly to tune font sizes, positions
// and text fitting without reflashing the device.

#include <Arduino_compat.h>

#include "global.h"

#if IOT_LVGL_SUPPORT && DEBUG_LVGL_SCREENSHOT

#include <functional>

namespace LVGLDebug {

// applies one "key:value" pair, returns true if the key was used
using SetValueCallback = std::function<bool(const String &key, const String &value)>;

// returns the list of keys/values shown on the debug page (plain text, without markup)
using GetValueHelpCallback = std::function<const char *()>;

// registers the callbacks of the application, call it from the plugin setup
void setValueCallback(SetValueCallback callback);
void setHelpCallback(GetValueHelpCallback callback);

// registers the HTTP handlers (/lvgl-screen and /lvgl-screen.bmp)
void setup();

// main loop hook, performs a pending screenshot
void loop();

// frees the buffers
void release();

} // namespace LVGLDebug

#endif
