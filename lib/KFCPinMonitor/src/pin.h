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
        HardwarePin(uint8_t pin, HardwarePinType type) :
            _pin(pin),
            _count(0),
            _type(type)
        {}
        virtual ~HardwarePin() {}

        virtual Debounce *getDebounce() const {
            return nullptr;
        }
        // virtual void handle() {}
        bool hasDebounce() const {
            return false;
        }

        // void updateState(uint32_t timeMicros, uint16_t intCount, bool value);
        virtual void updateState(uint32_t timeMicros, bool value) {}

        uint8_t getPin() const {
            return _pin;
        }

    protected:
        friend Monitor;
        friend RotaryEncoder;

        operator bool() const;
        uint8_t getCount() const;

        HardwarePin &operator++();
        HardwarePin &operator--();

        static void ICACHE_RAM_ATTR callback(void *arg);

        uint8_t _pin;
        uint8_t _count;
        HardwarePinType _type;
    };


    class SimpleHardwarePin : public HardwarePin {
    public:
        SimpleHardwarePin(uint8_t pin, HardwarePinType type = HardwarePinType::SIMPLE) :
            HardwarePin(pin, type),
            _micros(0),
            _intCount(0),
            _value(false)
            // _count(0)
        {
        }

        // virtual void handle() override {
        //     _micros = micros();
        //     _value = digitalRead(_pin);
        //     _intCount++;
        // }

        virtual void updateState(uint32_t timeMicros, bool value) override {
            _micros = timeMicros;
            _intCount++;
            _value = value;
        }

    protected:
        friend Monitor;
        friend RotaryEncoder;
        friend HardwarePin;

        volatile uint32_t _micros;
        volatile uint16_t _intCount: 15;
        volatile uint16_t _value: 1;
    };

    class DebouncedHardwarePin : public SimpleHardwarePin {
    public:
        DebouncedHardwarePin(uint8_t pin) :
            SimpleHardwarePin(pin, HardwarePinType::DEBOUNCE),
            _debounce(digitalRead(pin))
        {
        }

        virtual Debounce *getDebounce() const override {
            return const_cast<Debounce *>(&_debounce);
        }

        // void handle();

        virtual void updateState(uint32_t timeMicros, bool value) override {
            _micros = timeMicros;
            _intCount++;
            _value = value;
            _debounce.setState(_value);
        }

    protected:
        Debounce _debounce;
    };

    class RotaryHardwarePin : public HardwarePin {
    public:
        RotaryHardwarePin(uint8_t pin, Pin &handler) :
            HardwarePin(pin, HardwarePinType::ROTARY),
            _encoder(*reinterpret_cast<RotaryEncoder *>(const_cast<void *>(handler.getArg())))
        {}

        // virtual void handle() override;

    private:
        friend HardwarePin;

        RotaryEncoder &_encoder;
    };

    // inline HardwarePin::HardwarePin(uint8_t pin) :
    //     _micros(0),
    //     _intCount(0),
    //     _value(false),
    //     _pin(pin),
    //     _count(0)

    // {
    //     __DBG_printf("pin=%u debounce=%u", pin, false);
    // }

    inline HardwarePin::operator bool() const {
        return _count != 0;
    }

    inline uint8_t HardwarePin::getCount() const {
        return _count;
    }

    inline HardwarePin &HardwarePin::operator++() {
        ++_count;
        return *this;
    }

    inline HardwarePin &HardwarePin::operator--() {
        --_count;
        return *this;
    }

    // inline void HardwarePin::updateState(uint32_t timeMicros, uint16_t intCount, bool value)
    // {
    //     _micros = timeMicros;
    //     _intCount = intCount;
    //     _value = value;
    //     auto debounce = getDebounce();
    //     if (debounce) {
    //         debounce->setState(_value);
    //     }
    // }

    // inline void HardwarePin::updateState(uint32_t timeMicros, bool value)
    // {
    // }

}
