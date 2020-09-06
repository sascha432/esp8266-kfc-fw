/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>

#ifndef DEBUG_PIN_MONITOR
#define DEBUG_PIN_MONITOR                                       1
#endif

#if DEBUG_PIN_MONITOR

// display all events in intervals
#ifndef DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL
#define DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL                  0
// #define DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL                  1000
#endif

// display diebouncer info
#ifndef DEBUG_PIN_MONITOR_DEBOUNCER
#define DEBUG_PIN_MONITOR_DEBOUNCER                             0
#endif

// display pin events (rising, falling, high, low)
#ifndef DEBUG_PIN_MONITOR_EVENTS
#define DEBUG_PIN_MONITOR_EVENTS                                1
#endif


#endif

struct InterruptInfo;

namespace PinMonitor {

    static constexpr uint8_t kMaxPins = 17;

    class Pin;
    class Monitor;
    class Debounce;

    using TimeType = uint32_t;
    using TimeDiffType = int32_t;
    using DebouncePtr = std::unique_ptr<Debounce>;
    using PinPtr = std::unique_ptr<Pin>;
    using Vector = std::vector<PinPtr>;
    using Iterator = Vector::iterator;
    using Predicate = std::function<bool(const PinPtr &pin)>;

    enum class StateType : uint8_t {
        NONE,
        IS_RISING,
        IS_FALLING,
        IS_HIGH,
        IS_LOW
    };

    class PinUsage {
    public:
        PinUsage(uint8_t pin, uint16_t debounceTime);

        bool operator==(uint8_t pin) const {
            return _pin == pin;
        }

        operator bool() const {
            return _count != 0;
        }

        uint8_t getCount() const {
            return _count;
        }
        uint8_t getPin() const {
            return _pin;
        }

        PinUsage &operator++() {
            ++_count;
            return *this;
        }
        PinUsage &operator--() {
            ++_count;
            return *this;
        }

        Debounce &getDebounce();

    private:
        uint8_t _pin;
        uint8_t _count;
        DebouncePtr _debounce;
    };


    class ConfigType {
    public:
        ConfigType(uint8_t pin, bool activeLow = false) :
            _pin(pin),
            _disabled(false),
            _activeLow(activeLow)
        {
        }

        uint8_t getPin() const {
            return _pin;
        }

        bool isInverted() const {
            return _activeLow;
        }

        void setInverted(bool activeLow) {
            _activeLow = activeLow;
        }

        bool isEnabled() const {
            return _disabled == false;
        }

        // disable receiving events
        void setDisabled(bool disabled) {
            _disabled = disabled;
        }

    private:
        friend Monitor;
        friend Pin;

        bool _isActive(bool state) const{
            return state != _activeLow;
        }

        uint8_t _pin;
        bool _disabled: 1;
        bool _activeLow: 1;
    };

}
