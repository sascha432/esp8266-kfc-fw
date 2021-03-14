/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include "interrupt_impl.h"

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

#if DEBUG_PIN_MONITOR
        char *name() {
            snprintf_P(name_buffer, sizeof(name_buffer), PSTR("btn pin=%u"), _pin);
            return name_buffer;
        }

        virtual const char *name() const {
            return const_cast<Pin *>(this)->name();
        }
#endif

    public:

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
        uint8_t _pin: 4;
        bool _disabled: 1;
        bool _activeState: 1;

#if DEBUG_PIN_MONITOR
        char name_buffer[16]{};
#endif
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
        bool hasDebounce() const {
            return false;
        }

        uint8_t getPin() const {
            return _pin;
        }

    // protected:
    public:

        static void ICACHE_RAM_ATTR callback(void *arg);

        operator bool() const;
        uint8_t getCount() const;

        HardwarePin &operator++();
        HardwarePin &operator--();

        bool operator==(const uint8_t pin) const {
            return pin == _pin;
        }

        bool operator!=(const uint8_t pin) const {
            return pin != _pin;
        }

        uint8_t _pin;
        uint8_t _count;
        HardwarePinType _type;
    };

    class SimpleHardwarePin : public HardwarePin {
    public:
        SimpleHardwarePin(uint8_t pin,  HardwarePinType type = HardwarePinType::SIMPLE) : HardwarePin(pin, type) {}
    };

    class DebouncedHardwarePin : public SimpleHardwarePin {
    public:
        DebouncedHardwarePin(uint8_t pin, bool debounceValue) :
            SimpleHardwarePin(pin, HardwarePinType::DEBOUNCE),
            _debounce(debounceValue)
        {
        }

        virtual Debounce *getDebounce() const override {
            return const_cast<Debounce *>(&_debounce);
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

    public:
        RotaryEncoder &_encoder;
    };

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

}
