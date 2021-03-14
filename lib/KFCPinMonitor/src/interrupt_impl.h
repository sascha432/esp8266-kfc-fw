/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include "interrupt_event.h"
#include <stl_ext/array.h>
#include <stl_ext/type_traits.h>

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace PinMonitor {

    void GPIOInterruptsEnable();
    void GPIOInterruptsDisable();

    class RotaryEncoder;

    namespace Interrupt {

#ifdef PIN_MONITOR_PINS_TO_USE

        static constexpr auto kPins = stdex::array_of<const uint8_t>(PIN_MONITOR_PINS_TO_USE);

#endif

    }

    extern Interrupt::EventBuffer eventBuffer;

#if PIN_MONITOR_USE_GPIO_INTERRUPT

    extern uint16_t interrupt_levels;
    extern void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr);

#endif

}

#include <debug_helper_disable.h>
