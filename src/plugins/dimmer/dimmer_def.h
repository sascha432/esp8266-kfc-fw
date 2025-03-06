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
#    define IOT_DIMMER_MAX_BRIGHTNESS 1024
#endif

// use a timer to gradually adjust brightness in milliseconds
#ifndef IOT_DIMMER_TIMER_INTERVAL
#   define IOT_DIMMER_TIMER_INTERVAL 0
#endif

// map to range
#ifndef IOT_DIMMER_MAP_BRIGHTNESS_RANGE
#   define IOT_DIMMER_MAP_BRIGHTNESS_RANGE 100
#endif

// pwm default settings

// pwm range
#ifndef IOT_DIMMER_PWM_PWMRANGE
#   define IOT_DIMMER_PWM_PWMRANGE PWMRANGE
#endif

// pwm value for off
#ifndef IOT_DIMMER_PWM_OFF
#   define IOT_DIMMER_PWM_OFF 0
#endif

// min pwm
#ifndef IOT_DIMMER_PWM_RANGE_MIN
#   define IOT_DIMMER_PWM_RANGE_MIN 0
#endif

// max pwm
#ifndef IOT_DIMMER_PWM_RANGE_MAX
#   define IOT_DIMMER_PWM_RANGE_MAX IOT_DIMMER_PWM_PWMRANGE
#endif

// pwm frequency
#ifndef IOT_DIMMER_PWM_DAC_FREQ
#   define IOT_DIMMER_PWM_DAC_FREQ 10000
#endif

// custom mapping
#ifndef IOT_DIMMER_MAP_BRIGHTNESS
#   if KFC_DEVICE_ID == 76
/**
 * @brief custom mapping device #76
 *
 * 0 = 1023
 * 1 = 150
 * 255 = 0
 */
#       define IOT_DIMMER_MAP_BRIGHTNESS(value) (value == 0 ? IOT_DIMMER_PWM_OFF : (std::clamp<uint32_t>(IOT_DIMMER_PWM_RANGE_MAX - (((value - IOT_DIMMER_MIN_BRIGHTNESS) * (IOT_DIMMER_PWM_RANGE_MAX - IOT_DIMMER_PWM_RANGE_MIN)) / (IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS)), IOT_DIMMER_PWM_RANGE_MIN, IOT_DIMMER_PWM_RANGE_MAX)))
#   elif KFC_DEVICE_ID == 75
/**
 * @brief custom mapping device #75
 *
 * 0 = 100
 * 1 = 65
 * [...]
 * 255 = 45
 */
#       define IOT_DIMMER_MAP_BRIGHTNESS(value) std::clamp<uint32_t>((value ? (65 - (((value - IOT_DIMMER_MIN_BRIGHTNESS) * 20) / (IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS))) : 100), 45, 100)
#       else
#           error define mapping
// #   elif IOT_DIMMER_X9C_POTI
// #       define IOT_DIMMER_MAP_BRIGHTNESS(value) (((value - IOT_DIMMER_MIN_BRIGHTNESS) * IOT_DIMMER_MAP_BRIGHTNESS_RANGE) / (IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS))
// #   elif IOT_DIMMER_PWM_DAC
// #       define IOT_DIMMER_MAP_BRIGHTNESS(value) std::clamp<uint32_t>((((value - IOT_DIMMER_MIN_BRIGHTNESS) * (IOT_DIMMER_PWM_RANGE_MAX - IOT_DIMMER_PWM_RANGE_MIN)) / (IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS)), IOT_DIMMER_PWM_RANGE_MIN, IOT_DIMMER_PWM_RANGE_MAX)
#   endif
#endif
