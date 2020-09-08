/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"

namespace PinMonitor {

    class Pin
    {
    public:
        using StateType = PinMonitor::StateType;
        using ActiveStateType = PinMonitor::ActiveStateType;

        Pin() = delete;
        Pin(const Pin &) = delete;

        // arg can be used as identifier for removal
        // see Monitor::detach(const void *)
        Pin(uint8_t pin, const void *arg, StateType states = StateType::UP_DOWN, ActiveStateType activeState = ActiveStateType::PRESSED_WHEN_HIGH) :
            _arg(arg),
            _eventCounter(0),
            _states(states),
            _pin(pin),
            _disabled(false),
            _activeState(static_cast<bool>(activeState))
        {
        }
        virtual ~Pin() {}

        // event handlers are not executed more than once per millisecond
        virtual void event(StateType state, uint32_t now) {}

        // loop is called even if events are disabled
        virtual void loop() {}

#if DEBUG
        virtual void dumpConfig(Print &output);
#endif


    public:
        inline const void *getArg() const {
            return _arg;
        }

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
            return !_disabled;
        }

        inline bool isDisabled() const {
            return _disabled;
        }

        // disable receiving events
        inline void setDisabled(bool disabled) {
            _disabled = disabled;
        }

        // change events to receive
        inline void setEvents(StateType states) {
            _states = states;
        }

    private:
        friend Monitor;

        // return state if state is enabled and invert if _activeLow is true
        inline StateType _getStateIfEnabled(StateType state) const {
            return
                (static_cast<uint8_t>(_states) & static_cast<uint8_t>(state)) == static_cast<uint8_t>(StateType::NONE) ?
                    (StateType::NONE) :
                    (_activeState == static_cast<bool>(ActiveStateType::INVERTED) ? getInvertedState(state) : state);
        }

        const void *_arg;
        uint32_t _eventCounter;
        StateType _states;
        uint8_t _pin: 6;
        bool _disabled: 1;
        bool _activeState: 1;
    };

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
            --_count;
            return *this;
        }

        static void ICACHE_RAM_ATTR callback(void *arg);

        volatile uint32_t _micros;
        volatile uint16_t _intCount: 15;
        volatile uint16_t _value: 1;
        uint8_t _pin;
        uint8_t _count;
        Debounce _debounce;
    };

}
