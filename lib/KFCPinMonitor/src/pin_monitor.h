/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>

// Pin Monitor offers interruot bases PIN monitoring with support for toggle switches, push buttons, rotary encoders and deboucing.
// Arduino interrupt functons can be used or the optimized version of the pin monitor, which saves a couple hundret byte IRAM
// There is 3 options
// Arduino functional intrerrupts (>500byte IRAM)
// Arduino like functional interrupts with limitations (<350 byte IRAM)
// and an own implementation which requres less than 100 byte IRAM
// see PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS for details
//
// - toggle switches
// - push buttons with click, double click, multi click, long press, hold, hold repeat and hold release events
// - support for touch buttons
// - support for multi touch and button combinations (i.e. hold key1 + key4 for 5 seconds to reset the device)
// - rotary encoder with acceleration


#ifndef DEBUG_PIN_MONITOR
#define DEBUG_PIN_MONITOR                                       0
#endif

#if DEBUG_PIN_MONITOR
#ifndef DEBUG_PIN_MONITOR_EVENTS
#define DEBUG_PIN_MONITOR_EVENTS                                0
#endif
#else
#undef DEBUG_PIN_MONITOR_EVENTS
#define DEBUG_PIN_MONITOR_EVENTS                                0
#endif

// use custom interrupt handler for push buttons and rotary encoder
// other GPIO pins cannot be used
// lowest IRAM consumption and best performance
// no interrupt locking
#ifndef PIN_MONITOR_USE_GPIO_INTERRUPT
#define PIN_MONITOR_USE_GPIO_INTERRUPT                            0
#endif

// use attachInterruptArg()/detachInterrupt() for interrupt callbacks
// attachInterruptArg() requires 72 byte IRAM
// detachInterrupt() requires 160 byte IRAM
// = 232 byte IRAM
// if the arduino interrupt functions are not used anywhere else,
// set it 0. the custom implementation saves those 232 byte IRAM but
// is not fully compatible with arduino functional interrupts
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

// requires a list of PINs
// PINs are fixed and compiled in
#if PIN_MONITOR_USE_GPIO_INTERRUPT
#ifndef PIN_MONITOR_PINS_TO_USE
#error PIN_MONITOR_PINS_TO_USE not defined
#endif
#endif

// milliseconds
#ifndef PIN_MONITOR_DEBOUNCE_TIME
#define PIN_MONITOR_DEBOUNCE_TIME                               10
#endif

// suport for button groups
// allows to detect a single click across multiple buttons
#ifndef PIN_MONITOR_BUTTON_GROUPS
#define PIN_MONITOR_BUTTON_GROUPS                               0
#endif

// default input state for active
#ifndef PIN_MONITOR_ACTIVE_STATE
#define PIN_MONITOR_ACTIVE_STATE                                ActiveStateType::PRESSED_WHEN_HIGH
#endif

// max. number of events that can be stored before processing is requires
// the requires size depeneds on how often the queue is processed and how many events the buttons and encoders produce
#ifndef PIN_MONITOR_EVENT_QUEUE_SIZE
#define PIN_MONITOR_EVENT_QUEUE_SIZE                            64
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

