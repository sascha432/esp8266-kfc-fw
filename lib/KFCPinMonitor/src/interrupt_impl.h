/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include "interrupt_event.h"
#include <stl_ext/array.h>
#include <stl_ext/type_traits.h>

namespace PinMonitor {

// custom interrupt handler, requires least amount of IRAM
#if PIN_MONITOR_USE_GPIO_INTERRUPT || PIN_MONITOR_USE_POLLING

    extern void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr);

    namespace Interrupt {

        #ifndef PIN_MONITOR_PINS_TO_USE
        #error PIN_MONITOR_USE_GPIO_INTERRUPT=1 requires to define a list of pins used as PIN_MONITOR_PINS_TO_USE
        #endif
        static constexpr auto kPins = stdex::array_of<const uint8_t>(PIN_MONITOR_PINS_TO_USE);

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

#ifndef PIN_MONITOR_ETS_GPIO_INTR_DISABLE
#define PIN_MONITOR_ETS_GPIO_INTR_DISABLE()         ETS_GPIO_INTR_DISABLE();
#endif

#ifndef PIN_MONITOR_ETS_GPIO_INTR_ENABLE
#define PIN_MONITOR_ETS_GPIO_INTR_ENABLE()          ETS_GPIO_INTR_ENABLE();
#endif
