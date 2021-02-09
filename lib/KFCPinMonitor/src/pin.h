/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include <stl_ext/fixed_circular_buffer.h>

namespace PinMonitor {

    class RotaryEncoder;

    class Pin
    {
    public:
        using StateType = PinMonitor::StateType;
        using ActiveStateType = PinMonitor::ActiveStateType;

        Pin() = delete;
        Pin(const Pin &) = delete;

        // arg can be used as identifier for removal
        // see Monitor::detach(const void *)
        Pin(uint8_t pin, const void *arg, StateType states = StateType::UP_DOWN, ActiveStateType activeState = PIN_MONITOR_ACTIVE_STATE) :
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

        inline bool isPressed() const {
            auto tmp = _activeState == static_cast<bool>(ActiveStateType::INVERTED) ? getInvertedState(_states) : _states;
            return ((uint8_t)tmp & (uint8_t)StateType::DOWN) != (uint8_t)StateType::NONE;
        }

    private:
        friend Monitor;
        friend RotaryEncoder;

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
        HardwarePin(uint8_t pin);
        virtual ~HardwarePin() {}

        virtual Debounce *getDebounce() const;

        bool hasDebounce() const {
            return const_cast<HardwarePin *>(this)->getDebounce() != nullptr;
        }

        void updateState(uint32_t timeMicros, uint16_t intCount, bool value);
        void updateState(uint32_t timeMicros, bool value);
        uint8_t getPin() const;

    protected:
        friend Monitor;

        operator bool() const;
        uint8_t getCount() const;

    protected:

        HardwarePin &operator++();
        HardwarePin &operator--();

        static void ICACHE_RAM_ATTR callback(void *arg);

        volatile uint32_t _micros;
        volatile uint16_t _intCount: 15;
        volatile uint16_t _value: 1;
        uint8_t _pin;
        uint8_t _count;
    };

    class DebouncedHardwarePin : public HardwarePin {
    public:
        static constexpr uint16_t kIncrementCount = ~0;

        DebouncedHardwarePin(uint8_t pin);

        virtual Debounce *getDebounce() const override {
            return const_cast<Debounce *>(&_debounce);
            // return const_cast<Debounce *>(&const_cast<const Debounce &>(_debounce));
        }

    protected:
        Debounce _debounce;
    };

    class PinToggleState {
    public:
        PinToggleState(uint32_t micros = 0, bool state = 0) : _micros(micros), _state(state) {}

        uint32_t getTime() const {
            return _micros;
        }

        bool getState() const {
            return _state;
        }

        void set(uint32_t micros, bool state) {
            _micros = micros;
            _state = state;
        }

    private:
        uint32_t _micros;
        bool _state;
    };

    // class MultiHardwarePin : public HardwarePin {
    // public:
    //     MultiHardwarePin(uint8_t pin) : HardwarePin(pin) {}

    //     virtual void reset();
    //     virtual void ICACHE_RAM_ATTR interruptCallback(uint32_t time);


    // private:
    //
    // };

    inline HardwarePin::HardwarePin(uint8_t pin) :
        _micros(0),
        _intCount(0),
        _value(false),
        _pin(pin),
        _count(0)

    {
        __DBG_printf("pin=%u debounce=%u", pin, false);
    }

    inline HardwarePin::operator bool() const {
        return _count != 0;
    }

    inline uint8_t HardwarePin::getCount() const {
        return _count;
    }

    inline uint8_t HardwarePin::getPin() const {
        return _pin;
    }

    inline HardwarePin &HardwarePin::operator++() {
        ++_count;
        return *this;
    }

    inline HardwarePin &HardwarePin::operator--() {
        --_count;
        return *this;
    }

    inline void HardwarePin::updateState(uint32_t timeMicros, uint16_t intCount, bool value)
    {
        _micros = timeMicros;
        _intCount = intCount;
        _value = value;
        auto debounce = getDebounce();
        if (debounce) {
            debounce->setState(_value);
        }
    }

    inline void HardwarePin::updateState(uint32_t timeMicros, bool value)
    {
        _micros = timeMicros;
        _intCount++;
        _value = value;
        auto debounce = getDebounce();
        if (debounce) {
            debounce->setState(_value);
        }
    }

    inline DebouncedHardwarePin::DebouncedHardwarePin(uint8_t pin) :
        HardwarePin(pin),
        _debounce(digitalRead(pin))
    {
    }

}
