/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>

// requires 276/360 or 508/592 byte IRAM
// see PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS for details

#ifndef DEBUG_PIN_MONITOR
#define DEBUG_PIN_MONITOR                                       1
#endif

#if DEBUG_PIN_MONITOR
#ifndef DEBUG_PIN_MONITOR_EVENTS
#define DEBUG_PIN_MONITOR_EVENTS                                0
#endif
#else
#undef DEBUG_PIN_MONITOR_EVENTS
#define DEBUG_PIN_MONITOR_EVENTS                                0
#endif

// use custom interrupt handler for push buttons only
// other GPIO pins cannot be used
#ifndef PIN_MONITOR_USE_GPIO_INTERRUPT
#define PIN_MONITOR_USE_GPIO_INTERRUPT                            0
#endif
// use attachInterruptArg()/detachInterrupt() for interrupt callbacks
// attachInterruptArg() requires 72 byte IRAM
// detachInterrupt() requires 160 byte IRAM
// = 232 byte IRAM
// if the arduino interrupt functions are not used anywhere else,
// set it 0. the custom implementation saves those 232 byte IRAM but
// is not compatible with arduino functional interrupts
#ifndef PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
#if PIN_MONITOR_USE_GPIO_INTERRUPT
#define PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS                    0
#else
#define PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS                    1
#endif
#endif

#if PIN_MONITOR_USE_GPIO_INTERRUPT && PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
#error PIN_MONITOR_USE_GPIO_INTERRUPT=1 and PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS=1 cannot be combined
#endif

// milliseconds
#ifndef PIN_MONITOR_DEBOUNCE_TIME
#define PIN_MONITOR_DEBOUNCE_TIME                               10
#endif

// suport for button groups
#ifndef PIN_MONITOR_BUTTON_GROUPS
#define PIN_MONITOR_BUTTON_GROUPS                               0
#endif

// default input state for active
#ifndef PIN_MONITOR_ACTIVE_STATE
#define PIN_MONITOR_ACTIVE_STATE                                ActiveStateType::PRESSED_WHEN_HIGH
#endif

#if DEBUG_PIN_MONITOR
#define IF_DEBUG_PIN_MONITOR(...) __VA_ARGS__
#else
#define IF_DEBUG_PIN_MONITOR()
#endif

#include "debounce.h"

namespace PinMonitor {

    // all frequencies above (1000 / kDebounceTimeDefault) Hz will be filtered
    static constexpr uint8_t kDebounceTimeDefault = PIN_MONITOR_DEBOUNCE_TIME; // milliseconds

    class Pin;
    class Debounce;
    class Monitor;
    class HardwarePin;
    class SimpleHardwarePin;
    class DebouncedHardwarePin;
    class RotaryEncoder;
    class RotaryHardwarePin;

    using PinPtr = std::unique_ptr<Pin>;
    using Vector = std::vector<PinPtr>;
    using Iterator = Vector::iterator;
    using HardwarePinPtr = std::unique_ptr<HardwarePin>;
    using PinVector = std::vector<HardwarePinPtr>;
    using Predicate = std::function<bool(const PinPtr &pin)>;

    enum class HardwarePinType : uint8_t {
        NONE = 0,
        BASE,
        SIMPLE,
        DEBOUNCE,
        ROTARY,
        _DEFAULT = DEBOUNCE,
    };

    enum class RotaryEncoderEventType : uint8_t {
        NONE                            = 0,
        RIGHT                           = 0x10,
        CLOCK_WISE                      = RIGHT,
        LEFT                            = 0x20,
        COUNTER_CLOCK_WISE              = LEFT,
        ANY                             = 0xff,
        MAX_BITS                        = 8
    };

    enum class RotaryEncoderDirection : uint8_t {
        LEFT = 0,
        RIGHT = 1,
        LAST = RIGHT
    };

}

