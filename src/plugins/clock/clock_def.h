/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef IOT_CLOCK
#   error IOT_CLOCK missing
#endif

#ifndef IOT_LED_MATRIX
#   define IOT_LED_MATRIX 0
#endif

#if IOT_CLOCK && IOT_LED_MATRIX
#   define IOT_CLOCK_MODE 0
#   define IOT_MATRIX_MODE 1
#else
#   define IOT_CLOCK_MODE 1
#   define IOT_MATRIX_MODE 0
#endif

#if IOT_CLOCK_MODE
#    define IF_IOT_CLOCK_MODE(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_MODE(...)
#endif

#if IOT_MATRIX_MODE
#    define IF_IOT_MATRIX_MODE(...) __VA_ARGS__
#else
#    define IF_IOT_MATRIX_MODE(...)
#endif

#ifndef DEBUG_IOT_CLOCK
#    define DEBUG_IOT_CLOCK 1
#endif

#ifndef DEBUG_MEASURE_ANIMATION
#   define DEBUG_MEASURE_ANIMATION 1
#endif

// allows to diplay the RGB leds in the browser
#ifndef IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
#    define IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL 1
#endif

#if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL == 1 && HTTP2SERIAL_SUPPORT != 1
#   error HTTP2SERIAL_SUPPORT=1 required
#endif

#if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
#    define IF_IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL(...) __VA_ARGS__
#else
#    define IF_IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL(...)
#endif

// the number of pixels and order can be changed if set to 1
// configurable requires memory and CPU time of the maximum. number of configured LEDs, even if only 1 is active
// set IOT_LED_MATRIX_COLS=1 and IOT_LED_MATRIX_ROWS=max. number of LEDs
#ifndef IOT_LED_MATRIX_CONFIGURABLE
#   define IOT_LED_MATRIX_CONFIGURABLE 1
#endif

#ifndef IOT_CLOCK_PIXEL_MAPPING_TYPE
#   if IOT_LED_MATRIX_CONFIGURABLE
#       define IOT_CLOCK_PIXEL_MAPPING_TYPE DynamicPixelMapping
#   else
#       define IOT_CLOCK_PIXEL_MAPPING_TYPE PixelMapping
#   endif
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

#ifndef IOT_LED_MATRIX_WEBUI_COLSPAN_ANIMATION
#   define IOT_LED_MATRIX_WEBUI_COLSPAN_ANIMATION 3
#endif

#ifndef IOT_LED_MATRIX_WEBUI_COLSPAN_PROTECTION
#   define IOT_LED_MATRIX_WEBUI_COLSPAN_PROTECTION 3
#endif

// first pixel to use, others can be controlled separately and are reset during reboot only
#ifndef IOT_LED_MATRIX_PIXEL_OFFSET
#    define IOT_LED_MATRIX_PIXEL_OFFSET 0
#endif

// enable LED matrix mode instead of clock mode
#if IOT_LED_MATRIX
#   define IF_IOT_LED_MATRIX(...) __VA_ARGS__
#   define IF_IOT_CLOCK(...)
#   ifndef IOT_CLOCK_NUM_PIXELS
#       define IOT_CLOCK_NUM_PIXELS ((IOT_LED_MATRIX_COLS * IOT_LED_MATRIX_ROWS) + IOT_LED_MATRIX_PIXEL_OFFSET)
#   endif
#else
#    define IF_IOT_LED_MATRIX(...)
#    define IF_IOT_CLOCK(...) __VA_ARGS__
#    ifndef IOT_LED_MATRIX_COLS
#        define IOT_LED_MATRIX_COLS IOT_CLOCK_NUM_PIXELS
#        define IOT_LED_MATRIX_ROWS 1
#    endif
#endif

#if defined(IOT_LED_MATRIX_ROWS) && ((IOT_LED_MATRIX_COLS * IOT_LED_MATRIX_ROWS) + IOT_LED_MATRIX_PIXEL_OFFSET) != IOT_CLOCK_NUM_PIXELS
#error IOT_CLOCK_NUM_PIXELS does not match (IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS) + IOT_LED_MATRIX_PIXEL_OFFSET
#endif

#ifndef IOT_LED_MATRIX_OPTS_REVERSE_ROWS
#define IOT_LED_MATRIX_OPTS_REVERSE_ROWS true
#endif

#ifndef IOT_LED_MATRIX_OPTS_REVERSE_COLS
#define IOT_LED_MATRIX_OPTS_REVERSE_COLS false
#endif

#ifndef IOT_LED_MATRIX_OPTS_ROTATE
#define IOT_LED_MATRIX_OPTS_ROTATE false
#endif

#ifndef IOT_LED_MATRIX_OPTS_INTERLEAVED
#define IOT_LED_MATRIX_OPTS_INTERLEAVED true
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
// -1 to disable
#ifndef IOT_LED_MATRIX_ENABLE_PIN
#    define IOT_LED_MATRIX_ENABLE_PIN 15
#endif

// IOT_LED_MATRIX_ENABLE_PIN_INVERTED=1 sets IOT_LED_MATRIX_ENABLE_PIN to active low
#ifndef IOT_LED_MATRIX_ENABLE_PIN_INVERTED
#    define IOT_LED_MATRIX_ENABLE_PIN_INVERTED 0
#endif

// type of light sensor
// 0 = disabled
// 1 = sensor connected to ADC
// 2 = TinyPwm I2C sensor
#ifndef IOT_CLOCK_AMBIENT_LIGHT_SENSOR
#    define IOT_CLOCK_AMBIENT_LIGHT_SENSOR 0
#endif

// the higher the value, the brighter the ambient light. 1023 is maximum brightness (or the configured value for auto brightness)
// invert ADC result
#ifndef IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED
#define IOT_CLOCK_AMBIENT_LIGHT_SENSOR_INVERTED 0
#endif

// support for fan control
// 0 = disabled
// 1 = TinyPwm Fan Control
#ifndef IOT_LED_MATRIX_FAN_CONTROL
#   define IOT_LED_MATRIX_FAN_CONTROL 0
#endif

#if IOT_CLOCK_HAVE_OVERHEATED_PIN != -1
#       define IF_IOT_IOT_LED_OVERHEATED_PIN(...) __VA_ARGS__
#else
#       define IF_IOT_IOT_LED_OVERHEATED_PIN(...)
#endif

#ifndef IF_IOT_LED_MATRIX_FAN_CONTROL
#   if IOT_LED_MATRIX_FAN_CONTROL
#       define IF_IOT_LED_MATRIX_FAN_CONTROL(...) __VA_ARGS__
#   else
#       define IF_IOT_LED_MATRIX_FAN_CONTROL(...)
#   endif
#endif

#ifndef IF_IOT_LED_MATRIX_ENABLE_PIN_INVERTED
#   if IOT_LED_MATRIX_ENABLE_PIN_INVERTED
#       define IF_IOT_LED_MATRIX_ENABLE_PIN_INVERTED(a, b) (a)
#   else
#       define IF_IOT_LED_MATRIX_ENABLE_PIN_INVERTED(a, b) (b)
#   endif
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

#ifndef IOT_CLOCK_TEMPERATURE_PROTECTION
#   define IOT_CLOCK_TEMPERATURE_PROTECTION 0
#endif

#ifndef IF_IOT_CLOCK_TEMPERATURE_PROTECTION
#   if IOT_CLOCK_TEMPERATURE_PROTECTION
#       define IF_IOT_CLOCK_TEMPERATURE_PROTECTION(...) __VA_ARGS__
#   else
#       define IF_IOT_CLOCK_TEMPERATURE_PROTECTION(...)
#   endif
#endif

// address of the LM75A sensor for the voltage regulator
// 255 = none
#ifndef IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS
#    define IOT_CLOCK_VOLTAGE_REGULATOR_LM75A_ADDRESS 255
#endif

#if defined(ESP8266)
#    ifndef FASTLED_ESP8266_RAW_PIN_ORDER
#        define FASTLED_ESP8266_RAW_PIN_ORDER 1
#    endif
#endif

// UDP port for visualizer. 0 to disable
#ifndef IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
#   define IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER 0
#endif

#if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER
#   define IF_IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER(...) __VA_ARGS__
#else
#   define IF_IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER(...)
#endif

#ifndef IOT_SENSOR_HAVE_INA219
#   define IOT_SENSOR_HAVE_INA219 0
#endif

#if IOT_SENSOR_HAVE_INA219
#   define IF_IOT_SENSOR_HAVE_INA219(...) __VA_ARGS__
#else
#   define IF_IOT_SENSOR_HAVE_INA219(...)
#endif
