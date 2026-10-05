/**
 * Author: sascha_lammers@gmx.de
 */

// Switches the LED power off with the standby pin (MOSFET/relay, IOT_LED_MATRIX_STANDBY_PIN) when the
// firmware cannot do it with the LED transport: during boot before the safe mode is selected, and after a
// crash. WS2812 LEDs keep showing their last frame until they receive new data or lose power, so without
// this the LEDs stay on after a crash, during the core dump and in safe mode.
//
// Boards without a standby pin do nothing. The data lines are not touched, they are floating inputs after
// a reset.
//
// The hardware decides the state between a reset and bootOff(): the pin is an unconfigured input, the gate
// of the MOSFET needs a pull resistor to the "off" level.
//
//  bootOff()   normal context, ESP8266 preinit() and the start of setup() on the ESP32. Drives the pin to
//              "off", the plugin switches it on with _enable() or sets it to INPUT if the standby pin is
//              disabled in the configuration (standby_led).
//
//  set()       normal context, all other writes of the standby pin (ClockPlugin::_setLedPower() follows the
//              configuration), release() leaves it to the hardware, isOn() reads it
//
//  crashOff()  crash context, register writes only (IRAM on the ESP32), no Arduino/RTOS/flash access.
//              Only the output latch is written: a pin that the configuration left as input stays an input.
//              Called by the ESP8266 crash callback (ClockPluginClearPixels()) and on the ESP32 by the
//              wrapped panic handler (__wrap_esp_panic_handler, -Wl,--wrap=esp_panic_handler).

#pragma once

#include <Arduino_compat.h>
#include "clock_def.h"

namespace Clock {

    namespace LedPower {

        // all functions do nothing on boards without a standby pin

        // sets the level and switches the pin to output, no matter what the configuration says (standby_led).
        // Normal context only, ClockPlugin::_setLedPower() follows the configuration
        inline void set(bool on)
        {
            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                // set the level before switching to output, no glitch
                digitalWrite(IOT_LED_MATRIX_STANDBY_PIN, IOT_LED_MATRIX_STANDBY_PIN_STATE(on));
                pinMode(IOT_LED_MATRIX_STANDBY_PIN, OUTPUT);
            #else
                (void)on;
            #endif
        }

        // leaves the LED power to the hardware (pin is an input), the standby pin is disabled in the configuration
        inline void release()
        {
            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                pinMode(IOT_LED_MATRIX_STANDBY_PIN, INPUT);
            #endif
        }

        // true if the pin is at the "on" level
        inline bool isOn()
        {
            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                return digitalRead(IOT_LED_MATRIX_STANDBY_PIN) == IOT_LED_MATRIX_STANDBY_PIN_STATE(true);
            #else
                return false;
            #endif
        }

        // LED power off at boot before the safe mode is selected, normal context only
        inline void bootOff()
        {
            set(false);
        }

        // standby pin output latch set to "off", safe in a crash/panic handler
        void crashOff();

    }

}
