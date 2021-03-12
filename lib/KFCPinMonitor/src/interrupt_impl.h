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

#if PIN_MONITOR_USE_GPIO_INTERRUPT
#ifndef PIN_MONITOR_PINS_TO_USE
#error PIN_MONITOR_PINS_TO_USE not defined
#endif
#endif

namespace PinMonitor {

    void GPIOInterruptsEnable();
    void GPIOInterruptsDisable();

    class RotaryEncoder;

    namespace Interrupt {

#ifdef PIN_MONITOR_PINS_TO_USE

        static constexpr auto kPins = stdex::array_of<const uint8_t>(PIN_MONITOR_PINS_TO_USE);
        // static constexpr auto kPinTypes = std::array<const HardwarePinType, 4>({HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE});

#endif

    }

    extern Interrupt::EventBuffer eventBuffer;
    extern uint16_t interrupt_levels;

}

extern void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr);

#include <debug_helper_disable.h>
