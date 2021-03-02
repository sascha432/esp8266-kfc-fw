/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_DIMMER_MODULE
#define DEBUG_IOT_DIMMER_MODULE             1
#endif

// number of channels
#ifndef IOT_DIMMER_MODULE_CHANNELS
#define IOT_DIMMER_MODULE_CHANNELS          1
#endif

#ifndef DIMMER_CHANNEL_COUNT
#define DIMMER_CHANNEL_COUNT                IOT_DIMMER_MODULE_CHANNELS
#endif

// enable or disable buttons
#ifndef IOT_DIMMER_MODULE_HAS_BUTTONS
#define IOT_DIMMER_MODULE_HAS_BUTTONS       0
#endif

// use UART instead of I2C
#ifndef IOT_DIMMER_MODULE_INTERFACE_UART
#define IOT_DIMMER_MODULE_INTERFACE_UART    0
#endif

// max. brightness level
#ifndef IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#define IOT_DIMMER_MODULE_MAX_BRIGHTNESS    8192
#endif

#ifndef DIMMER_MAX_LEVEL
#define DIMMER_MAX_LEVEL                    IOT_DIMMER_MODULE_MAX_BRIGHTNESS
#endif

#ifndef IOT_DIMMER_MODULE_HOLD_REPEAT_TIME
#define IOT_DIMMER_MODULE_HOLD_REPEAT_TIME  125
#endif

// UART only change baud rate of the Serial port to match the dimmer module
#ifndef IOT_DIMMER_MODULE_BAUD_RATE
#define IOT_DIMMER_MODULE_BAUD_RATE         57600
#endif

// I2C only. SDA PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SDA
#define IOT_DIMMER_MODULE_INTERFACE_SDA     D3
#endif

// I2C only. SCL PIN
#ifndef IOT_DIMMER_MODULE_INTERFACE_SCL
#define IOT_DIMMER_MODULE_INTERFACE_SCL     D5
#endif

// pins for dimmer buttons
// each channel needs 2 buttons, up & down
#ifndef IOT_DIMMER_MODULE_BUTTONS_PINS
#define IOT_DIMMER_MODULE_BUTTONS_PINS      D6, D7
#endif

#ifndef IOT_DIMMER_MODULE_PINMODE
#define IOT_DIMMER_MODULE_PINMODE           INPUT
#endif

#ifndef IOT_SWITCH_PRESSED_STATE
#define IOT_SWITCH_PRESSED_STATE            PinMonitor::ActiveStateType::PRESSED_WHEN_LOW
#endif

