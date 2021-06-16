/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>

// Pin Monitor offers interrupt bases PIN monitoring with support for toggle switches, push buttons, rotary encoders with or without software debouncing
// Arduino interrupt functons can be used or the optimized version of the pin monitor, which saves a couple hundred byte IRAM
// all callbacks are scheduled in the main loop and not executed from the ISR
// if the main loop has too many delays, the monitoring can be installed as a timer and gets executed every 5ms. the timer callback is not an ISR and does
// not require any code to be in the IRAM
//
//  - Arduino functional interrupts (>500byte IRAM)
//  - Optimized Arduino like functional interrupts with some limitations (<350 byte IRAM)
//  - Custom implementation which requires less than 100 byte IRAM
// see PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS for details
//  - Polling mode without IRAM usage
//
// - toggle switches
// - push buttons with following events
//   - down
//   - up
//   - click
//   - double click
//   - multi click
//   - long press
//   - hold
//   - hold repeat
//   - hold released
// - push button groups that share timeouts for single/double click
// - support for touch buttons
// - support for multi touch and button combinations (i.e. hold key1 + key4 for 5 seconds to reboot the device)
// - 2 pin rotary encoders with acceleration
// - any kind of pin monitoring
//
// pins can be configured active low or active high globally, or set individually
// see PIN_MONITOR_ACTIVE_STATE


#ifndef DEBUG_PIN_MONITOR
#define DEBUG_PIN_MONITOR                                           0
#endif

// use custom interrupt handler for push buttons and rotary encoder
// other GPIO pins cannot be used
// lowest IRAM consumption and best performance
// no interrupt locking
// ---------------------------------------------------------------------------------------------
// NOTE: if functional interrupts are used, this will *increase* the IRAM usage since
// it does not share any code with the arduino implementation
// ---------------------------------------------------------------------------------------------
#ifndef PIN_MONITOR_USE_GPIO_INTERRUPT
#define PIN_MONITOR_USE_GPIO_INTERRUPT                              0
#endif

// polling key events with no interrupts to save IRAM
// rotary encoders require interrupts, but with polling enable the IRAM usage is very small
#ifndef PIN_MONITOR_USE_POLLING
#define PIN_MONITOR_USE_POLLING                                     0
#endif

// support for GPIO expanders and polling
#ifndef PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
#define PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT                   0
#endif

#if PIN_MONITOR_USE_POLLING == 0 && PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
#error GPIO expander pins only supported with polling
#endif

// up to 16 IO expander pins can be used with _digitalRead()
// the pins must be added to PIN_MONITOR_POLLING_GPIO_EXPANDER_PINS_TO_USE
#if !defined(PIN_MONITOR_POLLING_GPIO_EXPANDER_PINS_TO_USE) && PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
#error PIN_MONITOR_POLLING_GPIO_EXPANDER_PINS_TO_USE must be defined
#endif

// use attachInterruptArg()/detachInterrupt() for interrupt callbacks
// attachInterruptArg() requires 72 byte IRAM
// detachInterrupt() requires 160 byte IRAM
// = 232 byte IRAM
// ---------------------------------------------------------------------------------------------
// NOTE: if the arduino interrupt functions are *not* used anywhere else, set it to 0. the
// custom implementation saves those 232 byte IRAM but is not fully compatible with arduino
// functional interrupts
// ---------------------------------------------------------------------------------------------
#ifndef PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
#if PIN_MONITOR_USE_GPIO_INTERRUPT || PIN_MONITOR_USE_POLLING
#define PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS                       0
#else
#define PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS                       1
#endif
#endif

#if PIN_MONITOR_USE_GPIO_INTERRUPT && PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
#error PIN_MONITOR_USE_GPIO_INTERRUPT=1 and PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS=1 cannot be combined
#endif

#if (PIN_MONITOR_USE_GPIO_INTERRUPT || PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS) && PIN_MONITOR_USE_POLLING
#error PIN_MONITOR_USE_POLLING cannot be used with interrupts
#endif

// requires a list of PINs which are fixed and compiled in
#if PIN_MONITOR_USE_GPIO_INTERRUPT
#ifndef PIN_MONITOR_PINS_TO_USE
#error PIN_MONITOR_PINS_TO_USE not defined
#endif
#endif

// enable support for rotary encoders
// disabled by default since it requires IRAM
// PIN_MONITOR_USE_POLLING=1 does not support rotary encoders
// rotary encoders can still be used with interrupts and polling for other pins
#ifndef PIN_MONITOR_ROTARY_ENCODER_SUPPORT
#define PIN_MONITOR_ROTARY_ENCODER_SUPPORT                      0
#endif

// a list of pins that the rotary encoder uses
#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT && PIN_MONITOR_USE_POLLING
#ifndef PIN_MONITOR_ROTARY_ENCODER_PINS
#error PIN_MONITOR_ROTARY_ENCODER_PINS not defined
#endif
#endif

// support for debounced push buttons
// disabling will free some IRAM
#ifndef PIN_MONITOR_DEBOUNCED_PUSHBUTTON
#define PIN_MONITOR_DEBOUNCED_PUSHBUTTON                        1
#endif

// support for simple I/O pin
// disabled by default since it requires IRAM
#ifndef PIN_MONITOR_SIMPLE_PIN
#define PIN_MONITOR_SIMPLE_PIN                                  0
#endif

// debounce time in milliseconds
// values from 5-20ms are common
#ifndef PIN_MONITOR_DEBOUNCE_TIME
#define PIN_MONITOR_DEBOUNCE_TIME                               10
#endif

// support for button groups
// allows to detect single/dobule clicks across multiple buttons
#ifndef PIN_MONITOR_BUTTON_GROUPS
#define PIN_MONITOR_BUTTON_GROUPS                               0
#endif

// global default configuration
#ifndef PIN_MONITOR_ACTIVE_STATE
#define PIN_MONITOR_ACTIVE_STATE                                ActiveStateType::ACTIVE_HIGH
// #define PIN_MONITOR_ACTIVE_STATE                                ActiveStateType::ACTIVE_LOW
#endif

// max. number of events that can be stored before processing is required. old events get dropped
// each event requires 6 byte of RAM + the queue overhead (64 events = 396 byte, 16 = 108 byte)
//
// the required size depends on how often the queue is processed and how many events the buttons and encoders produce
// hardware debounces buttons will create 2 events, down and up while buttons with capacitors and pull-down/up resistors
// might produce a couple. rotary encoders should have enough capacity to reach the max. required turn speed.
// low quality buttons directly connected might create dozends or hundreds of events per keypress. this should be avoided
#ifndef PIN_MONITOR_EVENT_QUEUE_SIZE
#define PIN_MONITOR_EVENT_QUEUE_SIZE            64
#endif

#if DEBUG_PIN_MONITOR
#define IF_DEBUG_PIN_MONITOR(...) __VA_ARGS__
#else
#define IF_DEBUG_PIN_MONITOR()
#endif

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

    const __FlashStringHelper *getHardwarePinTypeStr(HardwarePinType type);

}

#include "interrupt_impl.h"
#include "debounce.h"
#include "pin.h"
#include "push_button.h"
#include "monitor.h"
#include "rotary_encoder.h"
