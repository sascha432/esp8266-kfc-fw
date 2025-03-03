/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef IOT_DIMMER_PLUGIN_NAME
#    define IOT_DIMMER_PLUGIN_NAME "dimmer"
#endif

#ifndef IOT_DIMMER_PLUGIN_FRIENDLY_NAME
#    define IOT_DIMMER_PLUGIN_FRIENDLY_NAME "Dimmer"
#endif

#ifndef IOT_DIMMER_TITLE
#    define IOT_DIMMER_TITLE "Dimmer"
#endif

#ifndef IOT_DIMMER_BRIGHTNESS_TITLE
#    define IOT_DIMMER_BRIGHTNESS_TITLE "Brightness"
#endif

// internal brightness
#ifndef IOT_DIMMER_MIN_BRIGHTNESS
#    define IOT_DIMMER_MIN_BRIGHTNESS 0
#endif

#ifndef IOT_DIMMER_MAX_BRIGHTNESS
#    define IOT_DIMMER_MAX_BRIGHTNESS 255
#endif

// map to range
#ifndef IOT_DIMMER_MAP_BRIGHTNESS_RANGE
#   define IOT_DIMMER_MAP_BRIGHTNESS_RANGE 100
#endif

// custom mapping
#ifndef IOT_DIMMER_MAP_BRIGHTNESS
// #   define IOT_DIMMER_MAP_BRIGHTNESS(value) (((value - IOT_DIMMER_MIN_BRIGHTNESS) * IOT_DIMMER_MAP_BRIGHTNESS_RANGE) / (IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS))
/**
 * 0 = 100
 * 1 = 65
 * [...]
 * 255 = 45
 */
#   define IOT_DIMMER_MAP_BRIGHTNESS(value)  (value ? (65 - (((value - IOT_DIMMER_MIN_BRIGHTNESS) * 20) / (IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS))) : 100)
#endif
