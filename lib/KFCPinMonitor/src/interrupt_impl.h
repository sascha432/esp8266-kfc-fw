/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <stl_ext/array.h>
#include <stl_ext/type_traits.h>
#if ESP8266
#include <interrupts.h>
#endif
#include "interrupt_event.h"

namespace PinMonitor {

    #if ESP8266

        struct GPIOInterruptLock {
            GPIOInterruptLock() {
                ETS_GPIO_INTR_DISABLE();
            }
            ~GPIOInterruptLock() {
                ETS_GPIO_INTR_ENABLE();
            }
        };

    #elif ESP32

        struct GPIOInterruptLock {
            GPIOInterruptLock() {
                portMuxLock mLock(_mux);
            }
            ~GPIOInterruptLock() {
                portMuxLock mLock(_mux);
            }
            static portMuxType _mux;
        };

    #endif

// custom interrupt handler, requires least amount of IRAM
#if PIN_MONITOR_USE_GPIO_INTERRUPT || PIN_MONITOR_USE_POLLING

    extern void IRAM_ATTR pin_monitor_interrupt_handler(void *ptr);

    namespace Interrupt {

        #ifndef PIN_MONITOR_PINS_TO_USE
            #error PIN_MONITOR_USE_GPIO_INTERRUPT=1 requires to define a list of pins used as PIN_MONITOR_PINS_TO_USE
        #endif
        static constexpr auto kPins = stdex::array_of<const uint8_t>(PIN_MONITOR_PINS_TO_USE);

        #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
            static constexpr auto kIOExpanderPins = stdex::array_of<const uint8_t>(PIN_MONITOR_POLLING_GPIO_EXPANDER_PINS_TO_USE);
        #endif

        #ifdef PIN_MONITOR_ROTARY_ENCODER_PINS
            static constexpr auto kRotaryPins = stdex::array_of<const uint8_t>(PIN_MONITOR_ROTARY_ENCODER_PINS);
            static constexpr auto kGPIORotaryMask = Interrupt::PinAndMask::mask_of(kRotaryPins);
        #endif

    }

// arduino style interrupt handler with limitations saving 232byte IRAM
#elif PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS == 0


    // saves 232 byte IRAM compared to attachInterruptArg/detachInterrupt
    // cannot be used with arduino functional interrupts
    // interrupt trigger is CHANGE only

    typedef void (* voidFuncPtrArg)(void *);

    // mode is CHANGE
    void _attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void *arg);
    // NOTE: this function must not be called from inside the interrupt handler
    void _detachInterrupt(uint8_t pin);

#endif

    void GPIOInterruptsEnable();
    void GPIOInterruptsDisable();

    class RotaryEncoder;

    extern uint16_t interrupt_levels;

#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
    extern Interrupt::EventBuffer eventBuffer;
#endif

}
