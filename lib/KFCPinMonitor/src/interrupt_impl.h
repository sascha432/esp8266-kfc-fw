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
#if PIN_MONITOR_USE_GPIO_INTERRUPT

    void GPIOInterruptsEnable();
    void GPIOInterruptsDisable();

    extern void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr);

    namespace Interrupt {

#ifndef PIN_MONITOR_PINS_TO_USE
#error PIN_MONITOR_USE_GPIO_INTERRUPT=1 requires to define a list of pins used as PIN_MONITOR_PINS_TO_USE
#endif
        static constexpr auto kPins = stdex::array_of<const uint8_t>(PIN_MONITOR_PINS_TO_USE);

    }

// arduino style interrupt handler with limitations saving 232byte IRAM
#elif PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS == 0

    // saves 232 byte IRAM compared to attachInterruptArg/detachInterrupt
    // cannot be used with arduino functional interrupts
    // interrupt trigger is CHANGE only

    typedef void (* voidFuncPtrArg)(void *);

    // mode is CHANGE
    void attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void *arg);
    // NOTE: this function must not be called from inside the interrupt handler
    void detachInterrupt(uint8_t pin);

#endif

    class RotaryEncoder;

    extern uint16_t interrupt_levels;
    extern Interrupt::EventBuffer eventBuffer;

}
