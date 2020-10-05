/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>

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

// milliseconds
#ifndef PIN_MONITOR_DEBOUNCE_TIME
#define PIN_MONITOR_DEBOUNCE_TIME                               10
#endif

#include "debounce.h"

namespace PinMonitor {

    // all frequencies above (1000 / kDebounceTimeDefault) Hz will be filtered
    static constexpr uint8_t kDebounceTimeDefault = PIN_MONITOR_DEBOUNCE_TIME; // milliseconds

    class Pin;
    class HardwarePin;
    class Debounce;
    class Monitor;

    using PinPtr = std::unique_ptr<Pin>;
    using Vector = std::vector<PinPtr>;
    using Iterator = Vector::iterator;
    using HardwarePinPtr = std::unique_ptr<HardwarePin>;
    using PinVector = std::vector<HardwarePinPtr>;
    using Predicate = std::function<bool(const PinPtr &pin)>;

}
