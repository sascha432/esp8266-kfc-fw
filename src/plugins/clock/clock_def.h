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
#    define DEBUG_IOT_CLOCK (0 || defined(DEBUG_ALL))
#endif

#ifndef DEBUG_MEASURE_ANIMATION
#   define DEBUG_MEASURE_ANIMATION DEBUG_IOT_CLOCK
#endif

// allows to display the RGB leds in the browser
#ifndef IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL
#    define IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL 0
#endif

#if IOT_CLOCK_VIEW_LED_OVER_HTTP2SERIAL == 1 && HTTP2SERIAL_SUPPORT != 1
#   error HTTP2SERIAL_SUPPORT=1 required
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

#ifndef IOT_LED_MATRIX_NEOPIXEL_SUPPORT
#    define IOT_LED_MATRIX_NEOPIXEL_SUPPORT 0
#endif

#ifndef IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
#    define IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT 1
#endif

#ifndef IOT_LED_MATRIX_FASTLED_ONLY
#    define IOT_LED_MATRIX_FASTLED_ONLY ((IOT_LED_MATRIX_NEOPIXEL_SUPPORT == 0) && (IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT == 0) ? 1 : 0)
#endif

// -1 to disable standby LED/Relay/MOSFET
#ifndef IOT_LED_MATRIX_STANDBY_PIN
#    define IOT_LED_MATRIX_STANDBY_PIN -1
#endif

// set to 1 for inverted/active low for the standby LED/Relay/MOSFET
#ifndef IOT_LED_MATRIX_STANDBY_PIN_INVERTED
#    define IOT_LED_MATRIX_STANDBY_PIN_INVERTED 0
#endif

#if IOT_LED_MATRIX_STANDBY_PIN_INVERTED
#    define IOT_LED_MATRIX_STANDBY_PIN_STATE(value) (value ? LOW : HIGH)
#else
#    define IOT_LED_MATRIX_STANDBY_PIN_STATE(value) (value ? HIGH : LOW)
#endif

// IR remote control PIN
#ifndef IOT_LED_MATRIX_IR_REMOTE_PIN
#    define IOT_LED_MATRIX_IR_REMOTE_PIN -1
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

#if defined(IOT_LED_MATRIX_ROWS) && ((IOT_LED_MATRIX_COLS * IOT_LED_MATRIX_ROWS) + IOT_LED_MATRIX_PIXEL_OFFSET) > IOT_CLOCK_NUM_PIXELS
#    error IOT_CLOCK_NUM_PIXELS is less than (IOT_LED_MATRIX_ROWS * IOT_LED_MATRIX_COLS) + IOT_LED_MATRIX_PIXEL_OFFSET
#endif

#ifndef IOT_LED_MATRIX_OPTS_REVERSE_ROWS
#    define IOT_LED_MATRIX_OPTS_REVERSE_ROWS true
#endif

#ifndef IOT_LED_MATRIX_OPTS_REVERSE_COLS
#    define IOT_LED_MATRIX_OPTS_REVERSE_COLS false
#endif

#ifndef IOT_LED_MATRIX_OPTS_ROTATE
#    define IOT_LED_MATRIX_OPTS_ROTATE false
#endif

#ifndef IOT_LED_MATRIX_OPTS_INTERLEAVED
#    define IOT_LED_MATRIX_OPTS_INTERLEAVED true
#endif

#if !NTP_CLIENT || !NTP_HAVE_CALLBACKS
#    error NTP_CLIENT=1 and NTP_HAVE_CALLBACKS=1 required
#endif

// pin for the multifunctional button
#ifndef IOT_CLOCK_BUTTON_PIN
#    define IOT_CLOCK_BUTTON_PIN -1
#endif

// pin for increasing brightness
#ifndef IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN
#    define IOT_LED_MATRIX_INCREASE_BRIGHTNESS_PIN -1
#endif

// pin for decreasing brightness
#ifndef IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN
#    define IOT_LED_MATRIX_DECREASE_BRIGHTNESS_PIN -1
#endif

// pin for toggling on/off
#ifndef IOT_LED_MATRIX_TOGGLE_PIN
#    define IOT_LED_MATRIX_TOGGLE_PIN -1
#endif

// 0 for no action
// 1 for switching to the next animation
#ifndef IOT_LED_MATRIX_TOGGLE_PIN_LONG_PRESS_TYPE
#    define IOT_LED_MATRIX_TOGGLE_PIN_LONG_PRESS_TYPE 0
#endif

// pin for next animation
#ifndef IOT_LED_MATRIX_NEXT_ANIMATION_PIN
#    define IOT_LED_MATRIX_NEXT_ANIMATION_PIN -1
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

// add sensor for calculated power level to webui/mqtt
#ifndef IOT_CLOCK_DISPLAY_POWER_CONSUMPTION
#   define IOT_CLOCK_DISPLAY_POWER_CONSUMPTION 0
#endif

#ifndef IOT_LED_MATRIX_WEBUI_COLSPAN_ANIMATION
#   define IOT_LED_MATRIX_WEBUI_COLSPAN_ANIMATION 3
#endif

#ifndef IOT_LED_MATRIX_WEBUI_COLSPAN_PROTECTION
#   define IOT_LED_MATRIX_WEBUI_COLSPAN_PROTECTION 3
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

//
// import math
//
// for f in range(1, 31):
//     def PF(P):
//         return max(0, P - (P * P * f / 1500.0))
//
//     print('// f=% 2.2s: ' % str(f), end='');
//     for P in (1, 2, 3, 4, 5, 7, 10, 15, 20, 25, 30, 35, 40, 50, 75, 100):
//         print('% 9.9s' % ('%.1f/%.0f' % (PF(P), P)), end=', ')
//     print()

// f= 1:     1.0/1,     2.0/2,     3.0/3,     4.0/4,     5.0/5,     7.0/7,    9.9/10,   14.8/15,   19.7/20,   24.6/25,   29.4/30,   34.2/35,   38.9/40,   48.3/50,   71.2/75,  93.3/100,
// f= 2:     1.0/1,     2.0/2,     3.0/3,     4.0/4,     5.0/5,     6.9/7,    9.9/10,   14.7/15,   19.5/20,   24.2/25,   28.8/30,   33.4/35,   37.9/40,   46.7/50,   67.5/75,  86.7/100,
// f= 3:     1.0/1,     2.0/2,     3.0/3,     4.0/4,     5.0/5,     6.9/7,    9.8/10,   14.6/15,   19.2/20,   23.8/25,   28.2/30,   32.5/35,   36.8/40,   45.0/50,   63.8/75,  80.0/100,
// f= 4:     1.0/1,     2.0/2,     3.0/3,     4.0/4,     4.9/5,     6.9/7,    9.7/10,   14.4/15,   18.9/20,   23.3/25,   27.6/30,   31.7/35,   35.7/40,   43.3/50,   60.0/75,  73.3/100,
// f= 5:     1.0/1,     2.0/2,     3.0/3,     3.9/4,     4.9/5,     6.8/7,    9.7/10,   14.2/15,   18.7/20,   22.9/25,   27.0/30,   30.9/35,   34.7/40,   41.7/50,   56.2/75,  66.7/100,
// f= 6:     1.0/1,     2.0/2,     3.0/3,     3.9/4,     4.9/5,     6.8/7,    9.6/10,   14.1/15,   18.4/20,   22.5/25,   26.4/30,   30.1/35,   33.6/40,   40.0/50,   52.5/75,  60.0/100,
// f= 7:     1.0/1,     2.0/2,     3.0/3,     3.9/4,     4.9/5,     6.8/7,    9.5/10,   13.9/15,   18.1/20,   22.1/25,   25.8/30,   29.3/35,   32.5/40,   38.3/50,   48.8/75,  53.3/100,
// f= 8:     1.0/1,     2.0/2,     3.0/3,     3.9/4,     4.9/5,     6.7/7,    9.5/10,   13.8/15,   17.9/20,   21.7/25,   25.2/30,   28.5/35,   31.5/40,   36.7/50,   45.0/75,  46.7/100,
// f= 9:     1.0/1,     2.0/2,     2.9/3,     3.9/4,     4.8/5,     6.7/7,    9.4/10,   13.7/15,   17.6/20,   21.2/25,   24.6/30,   27.6/35,   30.4/40,   35.0/50,   41.2/75,  40.0/100,
// f=10:     1.0/1,     2.0/2,     2.9/3,     3.9/4,     4.8/5,     6.7/7,    9.3/10,   13.5/15,   17.3/20,   20.8/25,   24.0/30,   26.8/35,   29.3/40,   33.3/50,   37.5/75,  33.3/100,
// f=11:     1.0/1,     2.0/2,     2.9/3,     3.9/4,     4.8/5,     6.6/7,    9.3/10,   13.3/15,   17.1/20,   20.4/25,   23.4/30,   26.0/35,   28.3/40,   31.7/50,   33.8/75,  26.7/100,
// f=12:     1.0/1,     2.0/2,     2.9/3,     3.9/4,     4.8/5,     6.6/7,    9.2/10,   13.2/15,   16.8/20,   20.0/25,   22.8/30,   25.2/35,   27.2/40,   30.0/50,   30.0/75,  20.0/100,
// f=13:     1.0/1,     2.0/2,     2.9/3,     3.9/4,     4.8/5,     6.6/7,    9.1/10,   13.1/15,   16.5/20,   19.6/25,   22.2/30,   24.4/35,   26.1/40,   28.3/50,   26.2/75,  13.3/100,
// f=14:     1.0/1,     2.0/2,     2.9/3,     3.9/4,     4.8/5,     6.5/7,    9.1/10,   12.9/15,   16.3/20,   19.2/25,   21.6/30,   23.6/35,   25.1/40,   26.7/50,   22.5/75,   6.7/100,
// f=15:     1.0/1,     2.0/2,     2.9/3,     3.8/4,     4.8/5,     6.5/7,    9.0/10,   12.8/15,   16.0/20,   18.8/25,   21.0/30,   22.8/35,   24.0/40,   25.0/50,   18.8/75,   0.0/100,
// f=16:     1.0/1,     2.0/2,     2.9/3,     3.8/4,     4.7/5,     6.5/7,    8.9/10,   12.6/15,   15.7/20,   18.3/25,   20.4/30,   21.9/35,   22.9/40,   23.3/50,   15.0/75,   0.0/100,
// f=17:     1.0/1,     2.0/2,     2.9/3,     3.8/4,     4.7/5,     6.4/7,    8.9/10,   12.4/15,   15.5/20,   17.9/25,   19.8/30,   21.1/35,   21.9/40,   21.7/50,   11.2/75,   0.0/100,
// f=18:     1.0/1,     2.0/2,     2.9/3,     3.8/4,     4.7/5,     6.4/7,    8.8/10,   12.3/15,   15.2/20,   17.5/25,   19.2/30,   20.3/35,   20.8/40,   20.0/50,    7.5/75,   0.0/100,
// f=19:     1.0/1,     1.9/2,     2.9/3,     3.8/4,     4.7/5,     6.4/7,    8.7/10,   12.2/15,   14.9/20,   17.1/25,   18.6/30,   19.5/35,   19.7/40,   18.3/50,    3.8/75,   0.0/100,
// f=20:     1.0/1,     1.9/2,     2.9/3,     3.8/4,     4.7/5,     6.3/7,    8.7/10,   12.0/15,   14.7/20,   16.7/25,   18.0/30,   18.7/35,   18.7/40,   16.7/50,    0.0/75,   0.0/100,
// f=21:     1.0/1,     1.9/2,     2.9/3,     3.8/4,     4.7/5,     6.3/7,    8.6/10,   11.8/15,   14.4/20,   16.2/25,   17.4/30,   17.9/35,   17.6/40,   15.0/50,    0.0/75,   0.0/100,
// f=22:     1.0/1,     1.9/2,     2.9/3,     3.8/4,     4.6/5,     6.3/7,    8.5/10,   11.7/15,   14.1/20,   15.8/25,   16.8/30,   17.0/35,   16.5/40,   13.3/50,    0.0/75,   0.0/100,
// f=23:     1.0/1,     1.9/2,     2.9/3,     3.8/4,     4.6/5,     6.2/7,    8.5/10,   11.6/15,   13.9/20,   15.4/25,   16.2/30,   16.2/35,   15.5/40,   11.7/50,    0.0/75,   0.0/100,
// f=24:     1.0/1,     1.9/2,     2.9/3,     3.7/4,     4.6/5,     6.2/7,    8.4/10,   11.4/15,   13.6/20,   15.0/25,   15.6/30,   15.4/35,   14.4/40,   10.0/50,    0.0/75,   0.0/100,
// f=25:     1.0/1,     1.9/2,     2.9/3,     3.7/4,     4.6/5,     6.2/7,    8.3/10,   11.2/15,   13.3/20,   14.6/25,   15.0/30,   14.6/35,   13.3/40,    8.3/50,    0.0/75,   0.0/100,
// f=26:     1.0/1,     1.9/2,     2.8/3,     3.7/4,     4.6/5,     6.2/7,    8.3/10,   11.1/15,   13.1/20,   14.2/25,   14.4/30,   13.8/35,   12.3/40,    6.7/50,    0.0/75,   0.0/100,
// f=27:     1.0/1,     1.9/2,     2.8/3,     3.7/4,     4.5/5,     6.1/7,    8.2/10,   10.9/15,   12.8/20,   13.8/25,   13.8/30,   12.9/35,   11.2/40,    5.0/50,    0.0/75,   0.0/100,
// f=28:     1.0/1,     1.9/2,     2.8/3,     3.7/4,     4.5/5,     6.1/7,    8.1/10,   10.8/15,   12.5/20,   13.3/25,   13.2/30,   12.1/35,   10.1/40,    3.3/50,    0.0/75,   0.0/100,
// f=29:     1.0/1,     1.9/2,     2.8/3,     3.7/4,     4.5/5,     6.1/7,    8.1/10,   10.7/15,   12.3/20,   12.9/25,   12.6/30,   11.3/35,    9.1/40,    1.7/50,    0.0/75,   0.0/100,
// f=30:     1.0/1,     1.9/2,     2.8/3,     3.7/4,     4.5/5,     6.0/7,    8.0/10,   10.5/15,   12.0/20,   12.5/25,   12.0/30,   10.5/35,    8.0/40,    0.0/50,    0.0/75,   0.0/100,

// -D IOT_CLOCK_POWER_CORRECTION_OUTPUT=P*0.9
// -D IOT_CLOCK_POWER_CORRECTION_OUTPUT=PF\(0.02\)
#ifndef IOT_CLOCK_POWER_CORRECTION_OUTPUT
#    define IOT_CLOCK_POWER_CORRECTION_OUTPUT PF(0.01)          // assume a non linear loss due to high currents
#endif

// support for motion sensor
#if IOT_SENSOR_HAVE_MOTION_SENSOR && !defined(IOT_CLOCK_MOTION_SENSOR_PIN)
#   error PIN not defined
#endif

#ifndef IOT_CLOCK_MOTION_SENSOR_PIN_INVERTED
#   define IOT_CLOCK_MOTION_SENSOR_PIN_INVERTED 0
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

// enable visualizer animation
#ifndef IOT_LED_MATRIX_ENABLE_VISUALIZER
#   define IOT_LED_MATRIX_ENABLE_VISUALIZER 0
#endif

#ifndef IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT
#    if IOT_LED_MATRIX_ENABLE_VISUALIZER
#        define IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT 21324
#    else
#        define IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT 0
#    endif
#endif

#ifndef IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
#    define IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE 0
#endif

#ifndef IOT_LED_MATRIX_I2S_MICROPHONE_SD
#    define IOT_LED_MATRIX_I2S_MICROPHONE_SD 32
#endif

#ifndef IOT_LED_MATRIX_I2S_MICROPHONE_WS
#    define IOT_LED_MATRIX_I2S_MICROPHONE_WS 15
#endif

#ifndef IOT_LED_MATRIX_I2S_MICROPHONE_SCK
#    define IOT_LED_MATRIX_I2S_MICROPHONE_SCK 14
#endif

#ifndef IOT_LED_MATRIX_I2S_PORT
#    define IOT_LED_MATRIX_I2S_PORT I2S_NUM_0
#endif

#if IOT_LED_MATRIX_ENABLE_VISUALIZER
#   define IF_IOT_LED_MATRIX_ENABLE_VISUALIZER(...) __VA_ARGS__
#else
#   define IF_IOT_LED_MATRIX_ENABLE_VISUALIZER(...)
#endif

#ifndef IOT_SENSOR_HAVE_INA219
#   define IOT_SENSOR_HAVE_INA219 0
#endif

#if IOT_SENSOR_HAVE_INA219
#   define IF_IOT_SENSOR_HAVE_INA219(...) __VA_ARGS__
#else
#   define IF_IOT_SENSOR_HAVE_INA219(...)
#endif
