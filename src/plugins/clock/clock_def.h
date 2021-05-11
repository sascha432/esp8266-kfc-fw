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

// standby LED is inverted/active low
#ifndef IOT_LED_MATRIX_STANDBY_PIN_INVERTED
#    define IOT_LED_MATRIX_STANDBY_PIN_INVERTED 0
#endif

#if IOT_LED_MATRIX_STANDBY_PIN_INVERTED
#    define IOT_LED_MATRIX_STANDBY_PIN_STATE(value) (value ? LOW : HIGH)
#else
#    define IOT_LED_MATRIX_STANDBY_PIN_STATE(value) (value ? HIGH : LOW)
#endif

// first pixel to use, others can be controlled separately and are reset during reboot only
#ifndef IOT_LED_MATRIX_START_ADDR
#    define IOT_LED_MATRIX_START_ADDR 0
#endif

// enable LED matrix mode instead of clock mode
#if IOT_LED_MATRIX
#    define IF_IOT_LED_MATRIX(...) __VA_ARGS__
#    define IF_IOT_CLOCK(...)
#else
#    define IF_IOT_LED_MATRIX(...)
#    define IF_IOT_CLOCK(...) __VA_ARGS__
#    ifndef IOT_LED_MATRIX_COLS
#        define IOT_LED_MATRIX_COLS IOT_CLOCK_NUM_PIXELS
#        define IOT_LED_MATRIX_ROWS 1
#    endif
#endif

#if SPEED_BOOSTER_ENABLED
#    error The speed booster causes timing issues and should be deactivated
#endif

#if !NTP_CLIENT || !NTP_HAVE_CALLBACKS
#    error NTP_CLIENT=1 and NTP_HAVE_CALLBACKS=1 required
#endif

// pin for the button
#ifndef IOT_CLOCK_BUTTON_PIN
#    define IOT_CLOCK_BUTTON_PIN 14
#endif

// save animation and brightness state
// if disabled, the configuration defaults are loaded if the device is restarted
#ifndef IOT_CLOCK_SAVE_STATE
#    define IOT_CLOCK_SAVE_STATE 1
#endif

#if IOT_CLOCK_BUTTON_PIN != -1
#    define IF_IOT_CLOCK_BUTTON_PIN(...) __VA_ARGS__
#    if !defined(PIN_MONITOR) || PIN_MONITOR == 0
#        error requires PIN_MONITOR=1
#    endif
#else
#    define IF_IOT_CLOCK_BUTTON_PIN(...)
#endif

#if IOT_CLOCK_SAVE_STATE
#    define IF_IOT_CLOCK_SAVE_STATE(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_SAVE_STATE(...)
#endif

#ifdef IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
#    define IF_IOT_LED_MATRIX_VIS(...) __VA_ARGS__
#else
#    define IF_IOT_LED_MATRIX_VIS(...)
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

#if IOT_CLOCK_BUTTON_PIN == -1 && IOT_CLOCK_HAVE_ROTARY_ENCODER
#    error IOT_CLOCK_HAVE_ROTARY_ENCODER requires IOT_CLOCK_BUTTON_PIN
#endif

// pins for the rotary encoder
#ifndef IOT_CLOCK_ROTARY_ENC_PINA
#    define IOT_CLOCK_ROTARY_ENC_PINA -1
#endif

#ifndef IOT_CLOCK_ROTARY_ENC_PINB
#    define IOT_CLOCK_ROTARY_ENC_PINB -1
#endif

// additional capacitive touch sensor for the rotary encoder
#ifndef IOT_CLOCK_TOUCH_PIN
#    define IOT_CLOCK_TOUCH_PIN -1
#endif

#if IOT_CLOCK_HAVE_ROTARY_ENCODER && (IOT_CLOCK_ROTARY_ENC_PINA == -1 || IOT_CLOCK_ROTARY_ENC_PINB == -1)
#    error IOT_CLOCK_ROTARY_ENC_PINA and IOT_CLOCK_ROTARY_ENC_PINB must be defined
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

#if IOT_CLOCK_HAVE_ENABLE_PIN
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

#ifndef IF_IOT_CLOCK_EN_PIN_INVERTED
#   if IOT_CLOCK_EN_PIN_INVERTED
#       define IF_IOT_CLOCK_EN_PIN_INVERTED(a, b) (a)
#   else
#       define IF_IOT_CLOCK_EN_PIN_INVERTED(a, b) (b)
#   endif
#endif

#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    define IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_AMBIENT_LIGHT_SENSOR(...)
#endif

// update interval in ms, 0 to disable
#if IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    ifndef IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL
#        define IOT_CLOCK_AUTO_BRIGHTNESS_INTERVAL 125
#    endif
#endif

// add sensor for calculated power level to webui/mqtt
#ifndef IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
#   define IOT_CLOCK_DISPLAY_POWER_CONSUMPTION 0
#endif

// update rate for webui
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
#   define IOT_CLOCK_HAVE_POWER_LIMIT 0
#endif

// since the power level is not linear, a correction formula can be applied
// float P = calculated power in watt
// <remarks>
// For further details check <see cref="./POWER_CALIBRATION.md" />
// </remarks>
//
// #define IOT_CLOCK_POWER_CORRECTION_OUTPUT <min_output_watt>, <formula>
// #define IOT_CLOCK_POWER_CORRECTION_OUTPUT 0.54, -0.131491 + 0.990507 * P + 0.015167 * P * P
//
// if IOT_CLOCK_POWER_CORRECTION_LIMIT is not defined, it uses IOT_CLOCK_POWER_CORRECTION_OUTPUT * 0.975
//
// #define IOT_CLOCK_POWER_CORRECTION_LIMIT <formula>
// #define IOT_CLOCK_POWER_CORRECTION_LIMIT -0.063393 + 0.920029 * P + 0.014318 * P * P
//
// for build_flags:
// -D IOT_CLOCK_POWER_CORRECTION_OUTPUT=0.54,-0.133437+0.990644*P+0.015165*P*P
// -D IOT_CLOCK_POWER_CORRECTION_LIMIT=-0.063393+0.920029*P+0.014318*P*P
#ifndef IOT_CLOCK_POWER_CORRECTION_OUTPUT
#    define IOT_CLOCK_POWER_CORRECTION_OUTPUT 1.0, P
#endif

#ifndef IOT_CLOCK_POWER_CORRECTION_LIMIT
#    define IOT_CLOCK_POWER_CORRECTION_LIMIT (P)
#endif

#if IOT_CLOCK_HAVE_POWER_LIMIT
#   define IF_IOT_CLOCK_HAVE_POWER_LIMIT(...) __VA_ARGS__
#else
#   define IF_IOT_CLOCK_HAVE_POWER_LIMIT(...)
#endif

// support for motion sensor
#ifndef IOT_CLOCK_HAVE_MOTION_SENSOR
#    define IOT_CLOCK_HAVE_MOTION_SENSOR 0
#endif

#if IOT_CLOCK_HAVE_MOTION_SENSOR
#    define IF_IOT_CLOCK_HAVE_MOTION_SENSOR(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_HAVE_MOTION_SENSOR(...)
#endif

#ifndef IOT_CLOCK_HAVE_MOTION_SENSOR_PIN
#    define IOT_CLOCK_HAVE_MOTION_SENSOR_PIN -1
#endif

// show rotating animation while the time is invalid
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

// address of the LM75A sensor for the voltage regulator
#ifndef IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS
#    define IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS -1
#endif

#if defined(ESP8266)
#    ifndef FASTLED_ESP8266_RAW_PIN_ORDER
#        define FASTLED_ESP8266_RAW_PIN_ORDER 1
#    endif
#endif

// UDP por3t for visualizer. 0 to disable
#ifndef IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
#define IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER 0
#endif
