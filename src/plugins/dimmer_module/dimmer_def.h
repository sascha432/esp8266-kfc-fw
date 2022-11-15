/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_DIMMER_MODULE
#    define DEBUG_IOT_DIMMER_MODULE (1 || defined(DEBUG_ALL))
#endif

// number of channels
#ifndef IOT_DIMMER_MODULE_CHANNELS
#    define IOT_DIMMER_MODULE_CHANNELS 1
#endif

#ifndef DIMMER_CHANNEL_COUNT
#    define DIMMER_CHANNEL_COUNT IOT_DIMMER_MODULE_CHANNELS
#endif

// enable or disable buttons
#ifndef IOT_DIMMER_MODULE_HAS_BUTTONS
#    define IOT_DIMMER_MODULE_HAS_BUTTONS 0
#endif

// use UART instead of I2C
#ifndef IOT_DIMMER_MODULE_INTERFACE_UART
#    define IOT_DIMMER_MODULE_INTERFACE_UART 0
#endif

// max. brightness level
#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#    define IOT_DIMMER_MODULE_MAX_BRIGHTNESS 8192
#endif

#ifndef DIMMER_MAX_LEVEL
#    define DIMMER_MAX_LEVEL IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#endif

// UART only change baud rate of the Serial port to match the dimmer module
#ifndef IOT_DIMMER_MODULE_BAUD_RATE
#    define IOT_DIMMER_MODULE_BAUD_RATE 57600
#endif

// default pins for dimmer buttons
// the buttons can be configured in the webui as well as the state active low/high
// each channel needs 2 buttons, up & down
#ifndef IOT_DIMMER_MODULE_BUTTONS_PINS
#    define IOT_DIMMER_MODULE_BUTTONS_PINS 4, 13
#endif

// repeat rate for hold button
#ifndef IOT_DIMMER_MODULE_HOLD_REPEAT_TIME
#    define IOT_DIMMER_MODULE_HOLD_REPEAT_TIME 250
#endif

#ifndef IOT_DIMMER_MODULE_PINMODE
#    define IOT_DIMMER_MODULE_PINMODE INPUT
#endif

// support for RGB dimming
// requires at least 3 channels
#ifndef IOT_DIMMER_HAS_RGB
#    define IOT_DIMMER_HAS_RGB 0
#endif

// support for RGB + white
// requires at least 4 channels
#ifndef IOT_DIMMER_HAS_RGBW
#    define IOT_DIMMER_HAS_RGBW 0
#endif

#if IOT_DIMMER_HAS_RGBW
#    ifdef IOT_DIMMER_HAS_RGB && !IOT_DIMMER_HAS_RGB
#        error IOT_DIMMER_HAS_RGB must be set to 1 if IOT_DIMMER_HAS_RGBW is set
#    endif
#    define IOT_DIMMER_HAS_RGB 1
#endif

// support for color temperature
// requires at least 2 channels
#ifndef IOT_DIMMER_HAS_COLOR_TEMP
#    define IOT_DIMMER_HAS_COLOR_TEMP 0
#endif

#ifndef IOT_DIMMER_HAVE_CHANNEL_ORDER
#    if IOT_ATOMIC_SUN_V2
#        define IOT_DIMMER_HAVE_CHANNEL_ORDER 1
#        define IOT_DIMMER_CHANNEL_ORDER      Base::_color._channel_ww1, Base::_color._channel_ww2, Base::_color._channel_cw1, Base::_color._channel_cw2
#    endif
#endif

// allow dimming of each channel even if it belongs to RGB or color temperature group
#ifndef IOT_DIMMER_SINGLE_CHANNELS
#    define IOT_DIMMER_SINGLE_CHANNELS 1
#endif

// adds a group switch for all channels to the title
#ifndef IOT_DIMMER_GROUP_SWITCH
#    define IOT_DIMMER_GROUP_SWITCH true
#endif

// add switch to lock channel groups
#ifndef IOT_DIMMER_GROUP_LOCK
#    define IOT_DIMMER_GROUP_LOCK false
#endif

#if !IOT_DIMMER_HAS_RGB && IOT_DIMMER_HAS_RGBW
#    error requires IOT_DIMMER_HAS_RGB=1
#endif

#if !IOT_DIMMER_HAS_RGB && !IOT_DIMMER_HAS_COLOR_TEMP && !IOT_DIMMER_SINGLE_CHANNELS
#    error requires IOT_DIMMER_SINGLE_CHANNELS=1 unless RGB or Color temperature is used
#endif

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

#if !defined(IOT_DIMMER_CHANNELS_TITLE) && IOT_DIMMER_SINGLE_CHANNELS
#    define IOT_DIMMER_CHANNELS_TITLE "Channels"
#endif

#if !defined(IOT_DIMMER_WBRIGHTNESS_TITLE) && IOT_DIMMER_HAS_RGBW
#    define IOT_DIMMER_WBRIGHTNESS_TITLE "White Brightness"
#endif

#if !defined(IOT_DIMMER_RGB_TITLE) && IOT_DIMMER_HAS_RGBW
#    define IOT_DIMMER_RGB_TITLE "Color"
#endif

#if !defined(IOT_DIMMER_COLOR_TEMP_TITLE) && IOT_DIMMER_HAS_COLOR_TEMP
#    define IOT_DIMMER_COLOR_TEMP_TITLE "Color Temperature"
#endif
