/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "polling_timer.h"
#include "gpio_interrupt_lock.h"

namespace PinMonitor {

    // --------------------------------------------------------------------
    // PinMonitor::Pin
    // --------------------------------------------------------------------

    class Pin
    {
    public:
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

        inline bool isActiveHigh() const {
            return _activeState;
        }

        inline bool isActiveLow() const {
            return !_activeState;
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
        uint8_t _pin;
        bool _disabled;
        bool _activeState;

#if DEBUG_PIN_MONITOR
        char name_buffer[16]{};
#endif
    };

    // --------------------------------------------------------------------
    // PinMonitor::HardwarePin
    // --------------------------------------------------------------------

    class HardwarePin {
    public:
        HardwarePin(uint8_t pin, HardwarePinType type) :
            _pin(pin),
            _count(0),
            _type(type)
        {}
        virtual ~HardwarePin() {}

        virtual void clear() {}

        virtual Debounce *getDebounce() const {
            return nullptr;
        }
        bool hasDebounce() const {
            return false;
        }

        inline uint8_t getPin() const {
            return _pin;
        }

        HardwarePinType getHardwarePinType() const  {
            return _type;
        }

        const __FlashStringHelper *getHardwarePinTypeStr() const  {
            return PinMonitor::getHardwarePinTypeStr(_type);
        }

    // protected:
    public:

#if PIN_MONITOR_USE_POLLING == 1
        static void callback(void *arg, uint16_t _GPI, uint16_t mask);
#elif PIN_MONITOR_USE_GPIO_INTERRUPT == 0
        static void IRAM_ATTR callback(void *arg);
#endif

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

    // --------------------------------------------------------------------
    // PinMonitor::SimpleHardwarePin
    // --------------------------------------------------------------------

    class SimpleHardwarePin : public HardwarePin {
    public:
        enum class SimpleEventType {
            NONE,
            HIGH_VALUE,
            LOW_VALUE,
        };

    public:
        SimpleHardwarePin(uint8_t pin,  HardwarePinType type = HardwarePinType::SIMPLE) :
            HardwarePin(pin, type),
            _event(SimpleEventType::NONE)
        {
        }

        virtual void clear() override {
            _event = SimpleEventType::NONE;
        }

        // void IRAM_ATTR addEvent(bool value) {
        inline __attribute__((__always_inline__))
        void addEvent(bool value) {
            _event = value ? SimpleEventType::HIGH_VALUE : SimpleEventType::LOW_VALUE;
        }

        inline void clearEvents() {
            GPIOInterruptLock lock;
            _event = SimpleEventType::NONE;
        }

        inline SimpleEventType getEvent() const {
            GPIOInterruptLock lock;
            auto tmp = _event;
            return tmp;
        }

        inline SimpleEventType getEventClear() {
            GPIOInterruptLock lock;
            auto tmp = _event;
            _event = SimpleEventType::NONE;
            return tmp;
        }

    private:
        // #if PIN_MONITOR_USE_POLLING == 0
        // volatile
        // #endif
        SimpleEventType _event;
    };

    // --------------------------------------------------------------------
    // PinMonitor::DebouncedHardwarePin
    // --------------------------------------------------------------------

    class DebouncedHardwarePin : public SimpleHardwarePin {
    public:
        // struct __attribute__((packed)) Events {
        //     uint32_t _micros;
        //     uint16_t _interruptCount: 15;
        //     bool _value: 1;
        //     Events() : _micros(0), _interruptCount(0), _value(0) {}
        // };
        struct Events {
            uint32_t _micros;
            uint16_t _interruptCount;
            bool _value;
            Events(uint32_t micros = 0, uint16_t interruptCount = 0, bool value = 0) :
                _micros(micros),
                _interruptCount(interruptCount),
                _value(value)
            {
            }

            void dump(Print &output) {
                output.printf_P(PSTR("micros=%u int_count=%u value=%u\n"), _micros, _interruptCount, _value);
            }
        };

    public:
        DebouncedHardwarePin(uint8_t pin, bool debounceValue) :
            SimpleHardwarePin(pin, HardwarePinType::DEBOUNCE),
            _debounce(debounceValue),
            _events(0)
        {
        }

        virtual void clear() override {
            GPIOInterruptLock lock;
            // _debounce = Debounce(digitalRead(getPin()));
            _debounce.setState(digitalRead(getPin()));
        }

        virtual Debounce *getDebounce() const override {
            return const_cast<Debounce *>(&_debounce);
        }

        inline __attribute__((__always_inline__))
        void addEvent(uint32_t micros, bool value) {
            GPIOInterruptLock lock;
            _events._micros = micros;
            _events._interruptCount++;
            _events._value = value;
        }

        inline __attribute__((__always_inline__))
        void clearEvents() {
            GPIOInterruptLock lock;
            _events._interruptCount = 0;
        }

        inline __attribute__((__always_inline__))
        Events getEvents() const {
            GPIOInterruptLock lock;
            return __getEvents();
        }

        inline __attribute__((__always_inline__))
        Events getEventsClear() {
            GPIOInterruptLock lock;
            auto tmp = __getEvents();
            clear();
            return tmp;
        }

    protected:
        // GPIO interrupts must be disabled when calling this method
        inline __attribute__((__always_inline__))
        Events __getEvents() const {
            auto tmp = Events(_events._micros, _events._interruptCount, _events._value);
            return tmp;
        }

    protected:
        Debounce _debounce;
        Events _events;
    };

    // --------------------------------------------------------------------
    // PinMonitor::RotaryHardwarePin
    // --------------------------------------------------------------------

    class RotaryHardwarePin : public HardwarePin {
    public:
        RotaryHardwarePin(uint8_t pin, Pin &handler) :
            HardwarePin(pin, HardwarePinType::ROTARY),
            _encoder(*reinterpret_cast<RotaryEncoder *>(const_cast<void *>(handler.getArg())))
        {}

        // virtual void clear() override {
            // auto iterator = std::remove_if(PinMonitor::eventBuffer.begin(), PinMonitor::eventBuffer.end(), [this](const PinPtr &ptr) {
            //     reeturn ptr->getPin() == _pin;

            // });
            // if (iterator != PinMonitor::eventBuffer.end()) {
            //     PinMonitor::eventBuffer.shrink(PinMonitor::eventBuffer.begin(), iterator);
            // }

            // PinMonitor::eventBuffer.shrink(PinMonitor::eventBuffer.begin(), std::remove_if(PinMonitor::eventBuffer.begin(), PinMonitor::eventBuffer.end(), [this](const PinPtr &ptr) {
            //     reeturn ptr->getPin() == _pin;
            // }));
        // }

    public:
        RotaryEncoder &_encoder;
    };

    // --------------------------------------------------------------------
    // PinMonitor::HardwarePin
    // --------------------------------------------------------------------

    inline HardwarePin::operator bool() const
    {
        return _count != 0;
    }

    inline uint8_t HardwarePin::getCount() const
    {
        return _count;
    }

    inline HardwarePin &HardwarePin::operator++()
    {
        ++_count;
        return *this;
    }

    inline HardwarePin &HardwarePin::operator--()
    {
        --_count;
        return *this;
    }

}
