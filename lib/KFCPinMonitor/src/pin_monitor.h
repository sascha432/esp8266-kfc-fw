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

    class HardwarePin {
    public:
        // HardwarePin(HardwarePin &&move) noexcept;
        // HardwarePin &operator=(HardwarePin &&move) noexcept;

        HardwarePin(uint8_t pin) :
            _micros(0),
            _intCount(0),
            _value(false),
            _pin(pin),
            _count(0),
            _debounce(digitalRead(pin))
        {
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

        HardwarePin &operator++() {
            ++_count;
            return *this;
        }
        HardwarePin &operator--() {
            ++_count;
            return *this;
        }

        Debounce &getDebounce()
        {
            return _debounce;
        }

        static void ICACHE_RAM_ATTR callback(void *arg);

    private:
        friend Monitor;

        volatile TimeType _micros;
        volatile uint16_t _intCount: 15;
        volatile uint16_t _value: 1;
        uint8_t _pin;
        uint8_t _count;
        Debounce _debounce;
    };

    class ConfigType {
    public:
        ConfigType(uint8_t pin, StateType states = StateType::HIGH_LOW, bool activeLow = false) :
            _states(states),
            _pin(pin),
            _disabled(false),
            _activeLow(activeLow)
        {
        }
        virtual ~ConfigType() {}

        uint8_t getPin() const {
            return _pin;
        }

        bool isInverted() const {
            return _activeLow;
        }

        void setInverted(bool inverted) {
            _activeLow = inverted;
        }

        bool isEnabled() const {
            return _disabled == false;
        }

        // disable receiving events
        void setDisabled(bool disabled) {
            _disabled = disabled;
        }

        void setStates(StateType states) {
            _states = states;
        }

    protected:
        friend Monitor;

        // return state if state is enabled and invert if _activeLow is true
        StateType _getState(StateType state) const {
            return
                (((uint8_t)_states & (uint8_t)state)) ?
                    (_activeLow ? _invertState(state) : state) :
                    StateType::NONE;
        }

        static inline StateType _invertState(StateType state) {
            auto tmp = (uint8_t)state;
            // swap pairs of bits
            return (StateType)(((tmp & 0xaa) >> 1) | ((tmp & 0x55) << 1));
        }

        StateType _states;
        uint8_t _pin: 6;
        bool _disabled: 1;
        bool _activeLow: 1;
    };

}
