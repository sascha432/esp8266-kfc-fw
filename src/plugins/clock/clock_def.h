/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_CLOCK
#    define DEBUG_IOT_CLOCK 1
#endif

#ifndef IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
#    define IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL 1
#endif

// -1 to disable standby LED
#ifndef IOT_LED_MATRIX_STANDBY_PIN
#    define IOT_LED_MATRIX_STANDBY_PIN -1
#endif

#if IOT_LED_MATRIX_STANDBY_PIN != -1
#    define IF_IOT_LED_MATRIX_STANDBY_PIN(...) __VA_ARGS__
#else
#    define IF_IOT_LED_MATRIX_STANDBY_PIN(...)
#endif

#ifndef IOT_LED_MATRIX_STANDBY_PIN_INVERTED
#    define IOT_LED_MATRIX_STANDBY_PIN_INVERTED 0
#endif

#if IOT_LED_MATRIX_STANDBY_PIN_INVERTED
#    define IOT_LED_MATRIX_STANDBY_PIN_STATE(value) (value ? LOW : HIGH)
#else
#    define IOT_LED_MATRIX_STANDBY_PIN_STATE(value) (value ? HIGH : LOW)
#endif

#if IOT_LED_MATRIX

#    define IF_IOT_LED_MATRIX(...) __VA_ARGS__
#    define IF_IOT_CLOCK(...)


#    define IOT_CLOCK_NUM_PIXELS (IOT_LED_MATRIX_COLS * IOT_LED_MATRIX_ROWS)

// first pixel to use, others can be controlled separately and are reset during reboot only
#    ifndef IOT_LED_MATRIX_START_ADDR
#        define IOT_LED_MATRIX_START_ADDR 0
#    endif

#else

#    define IF_IOT_LED_MATRIX(...)
#    define IF_IOT_CLOCK(...) __VA_ARGS__

// number of digits
#    ifndef IOT_CLOCK_NUM_DIGITS
#        define IOT_CLOCK_NUM_DIGITS 4
#    endif

// pixels per segment
#    ifndef IOT_CLOCK_NUM_PIXELS
#        define IOT_CLOCK_NUM_PIXELS 2
#    endif

#    ifndef IOT_LED_MATRIX_COLS
#        define IOT_LED_MATRIX_COLS IOT_CLOCK_NUM_PIXELS
#        define IOT_LED_MATRIX_ROWS 1
#    endif

// number of colons
#    ifndef IOT_CLOCK_NUM_COLONS
#        if IOT_CLOCK_NUM_DIGITS == 4
#            define IOT_CLOCK_NUM_COLONS 1
#        else
#            define IOT_CLOCK_NUM_COLONS 2
#        endif
#    endif

// pixels per dot
#    ifndef IOT_CLOCK_NUM_COLON_PIXELS
#        define IOT_CLOCK_NUM_COLON_PIXELS 2
#    endif

// order of the segments (a-g)
#    ifndef IOT_CLOCK_SEGMENT_ORDER
#        define IOT_CLOCK_SEGMENT_ORDER { 0, 1, 3, 4, 5, 6, 2 }
#    endif

// digit order, 30=colon #1,31=#2, etc...
#    ifndef IOT_CLOCK_DIGIT_ORDER
#        define IOT_CLOCK_DIGIT_ORDER { 0, 1, 30, 2, 3, 31, 4, 5 }
#    endif

// pixel order of pixels that belong to digits, 0 to use pixel addresses of the 7 segment class
#    ifndef IOT_CLOCK_PIXEL_ORDER
#        define IOT_CLOCK_PIXEL_ORDER { 0 }
#    endif

#endif

#if SPEED_BOOSTER_ENABLED
#    error The speed booster causes timing issues and should be deactivated
#endif

#if !NTP_CLIENT || !NTP_HAVE_CALLBACKS
#    error NTP_CLIENT=1 and NTP_HAVE_CALLBACKS=1 required
#endif

#ifndef IOT_CLOCK_BUTTON_PIN

#    define IOT_CLOCK_BUTTON_PIN 14
#    ifndef IOT_CLOCK_SAVE_STATE
#        define IOT_CLOCK_SAVE_STATE 1
#    endif

#else

#    ifndef IOT_CLOCK_SAVE_STATE
#        define IOT_CLOCK_SAVE_STATE 0
#    endif

#endif

#if IOT_CLOCK_BUTTON_PIN != -1
#    define IF_IOT_CLOCK_BUTTON_PIN(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_BUTTON_PIN(...)
#endif

#if IOT_CLOCK_SAVE_STATE
#    define IF_IOT_CLOCK_SAVE_STATE(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_SAVE_STATE(...)
#endif

// delay in seconds before any changes get stored except for power on/off
// prevents from wearing out the EEPROM
#ifndef IOT_CLOCK_SAVE_STATE_DELAY
#    define IOT_CLOCK_SAVE_STATE_DELAY 30
#endif


// enable support for a rotary encoder
#ifndef IOT_CLOCK_HAVE_ROTARY_ENCODER
#    define IOT_CLOCK_HAVE_ROTARY_ENCODER 0
#endif

#if IOT_CLOCK_BUTTON_PIN == -1
#    error requires IOT_CLOCK_BUTTON_PIN
#endif

// pins for the rotary encoder
#ifndef IOT_CLOCK_ROTARY_ENC_PINA
#    define IOT_CLOCK_ROTARY_ENC_PINA -1
#endif

#ifndef IOT_CLOCK_ROTARY_ENC_PINB
#    define IOT_CLOCK_ROTARY_ENC_PINB -1
#endif

#if IOT_CLOCK_HAVE_ROTARY_ENCODER
#    define IF_IOT_CLOCK_HAVE_ROTARY_ENCODER(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_HAVE_ROTARY_ENCODER(...)
#endif

// enable/disable power to all LEDs per GPIO
#ifndef IOT_CLOCK_HAVE_ENABLE_PIN
#    define IOT_CLOCK_HAVE_ENABLE_PIN 0
#endif

#if IF_IOT_CLOCK_HAVE_ENABLE_PIN
#    define IF_IOT_CLOCK_HAVE_ENABLE_PIN(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_HAVE_ENABLE_PIN(...)
#endif

// pin to enable LEDs
#ifndef IOT_CLOCK_EN_PIN
#    define IOT_CLOCK_EN_PIN 15
#endif

// IOT_CLOCK_EN_PIN_INVERTED=1 sets IOT_CLOCK_EN_PIN to active low
#ifndef IOT_CLOCK_EN_PIN_INVERTED
#    define IOT_CLOCK_EN_PIN_INVERTED 0
#endif

// disable ambient light sensor by default
#ifndef IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    define IOT_CLOCK_AMBIENT_LIGHT_SENSOR 0
#endif

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    define IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(...)
#endif

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    ifndef IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
// update interval in ms, 0 to disable
#        define IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL 125
#    endif
#endif

// add sensor for calculated power level
#ifndef IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
#   define IOT_CLOCK_DISPLAY_POWER_CONSUMPTION 1
#endif

#ifndef IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE
#   define IOT_CLOCK_CALC_POWER_CONSUMPTION_UPDATE_RATE 2
#endif

#if IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
#    define IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_DISPLAY_POWER_CONSUMPTION(...)
#endif

// option to set a power limit
#ifndef IOT_CLOCK_HAVE_POWER_LIMIT
#   define IOT_CLOCK_HAVE_POWER_LIMIT 1
#endif

#if IF_IOT_CLOCK_HAVE_POWER_LIMIT
#   define IF_IOT_CLOCK_HAVE_POWER_LIMIT(...) __VA_ARGS__
#else
#   define IF_IOT_CLOCK_HAVE_POWER_LIMIT(...)
#endif

// show rotating animation while time is invalid
#ifndef IOT_CLOCK_PIXEL_SYNC_ANIMATION
#    define IOT_CLOCK_PIXEL_SYNC_ANIMATION 0
#endif
#if IOT_LED_MATRIX && IOT_CLOCK_PIXEL_SYNC_ANIMATION
#    error not supported, set IOT_CLOCK_PIXEL_SYNC_ANIMATION=0
#endif

#if IOT_CLOCK_PIXEL_SYNC_ANIMATION
#    define IF_IOT_CLOCK_PIXEL_SYNC_ANIMATION(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_PIXEL_SYNC_ANIMATION(...)
#endif
