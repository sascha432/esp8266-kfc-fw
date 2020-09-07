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
        HardwarePin(uint8_t pin) :
            _micros(0),
            _intCount(0),
            _value(false),
            _pin(pin),
            _count(0),
            _debounce(digitalRead(pin))
        {
        }

        inline Debounce &getDebounce()
        {
            return _debounce;
        }


    private:
        friend Monitor;

        inline operator bool() const {
            return _count != 0;
        }

        inline uint8_t getCount() const {
            return _count;
        }
        inline uint8_t getPin() const {
            return _pin;
        }

        inline HardwarePin &operator++() {
            ++_count;
            return *this;
        }
        inline HardwarePin &operator--() {
            ++_count;
            return *this;
        }

        static void ICACHE_RAM_ATTR callback(void *arg);

        volatile TimeType _micros;
        volatile uint16_t _intCount: 15;
        volatile uint16_t _value: 1;
        uint8_t _pin;
        uint8_t _count;
        Debounce _debounce;
    };

    class ConfigType {
    public:
        ConfigType(uint8_t pin, StateType states = StateType::UP_DOWN, ActiveStateType activeState = ActiveStateType::PRESSED_WHEN_LOW) :
            _states(states),
            _pin(pin),
            _disabled(false),
            _activeState(static_cast<bool>(activeState))
        {
        }
        virtual ~ConfigType() {}

        inline uint8_t getPin() const {
            return _pin;
        }

        inline bool isInverted() const {
            return _activeState == static_cast<bool>(ActiveStateType::INVERTED);
        }

        inline void setInverted(bool inverted) {
            _activeState =  static_cast<bool>(inverted ? ActiveStateType::INVERTED : ActiveStateType::NON_INVERTED);
        }

        inline void setActiveState(ActiveStateType activeState) {
            _activeState = static_cast<bool>(activeState);
        }

        inline bool isEnabled() const {
            return _disabled == false;
        }

        // disable receiving events
        inline void setDisabled(bool disabled) {
            _disabled = disabled;
        }

        // change events to receive
        inline void setEvents(StateType states) {
            _states = states;
        }

    protected:
        friend Monitor;

        // return state if state is enabled and invert if _activeLow is true
        inline StateType _getStateIfEnabled(StateType state) const {
            return
                (static_cast<uint8_t>(_states) & static_cast<uint8_t>(state)) == static_cast<uint8_t>(StateType::NONE) ?
                    (StateType::NONE) :
                    (_activeState == static_cast<bool>(ActiveStateType::INVERTED) ? getInvertedState(state) : state);
        }

        StateType _states;
        uint8_t _pin: 6;
        bool _disabled: 1;
        bool _activeState: 1;
    };

}
