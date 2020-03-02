/**
* Author: sascha_lammers@gmx.de
*/

#ifndef ESP32

#include <EventTimer.h>
#include <EventScheduler.h>
#include <FunctionalInterrupt.h>
#include "VirtualPinDebug.h"

#if DEBUG_VIRTUAL_PIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class InterruptHandler {
public:
    typedef std::function<void(void)> *intRoutinePtr;
    typedef std::function<void(InterruptInfo)> *scheduledIntRoutinePtr;
    typedef std::vector<InterruptHandler> InterruptHandlerVector;

    enum class FuncType {
        FUNC,
        SCHEDULED,
    };

    InterruptHandler() : _callback(nullptr) {
    }
    InterruptHandler(uint8_t pin, void *callback, FuncType type, int mode) : _pin(pin), _callback(callback), _type(type), _mode(mode) {
        _debug_printf_P(PSTR("pin=%u callback=%p type=%u mode=%u\n"), _pin, _callback, _type, _mode);
    }

    void dumpHandler(Print &output) {
        output.printf_P(PSTR("pin=%u callback=%p type=%u mode=%u\n"), _pin, _callback, _type, _mode);
    }

private:
    uint8_t _pin;
    void *_callback;
    FuncType _type;
    int _mode;

public:
    static void addHandler(const InterruptHandler &handler) {
        removeHandler(handler._pin);
        _interruptHandlers.push_back(handler);
    }

    static InterruptHandlerVector::iterator getHandler(void *callback, FuncType type) {
        return std::find_if(_interruptHandlers.begin(), _interruptHandlers.end(), [callback, type](const InterruptHandler &handler) {
            return (handler._type == type) && (handler._callback == callback);
        });
    }

    // static void removeHandler(void *callback, FuncType type) {
    //     _interruptHandlers.erase(std::remove_if(_interruptHandlers.begin(), _interruptHandlers.end(), [callback, type](const InterruptHandler &handler) {
    //         return (handler._type == type) && (handler._callback == callback);
    //     }));
    // }
    static void removeHandler(uint8_t pin) {
        _debug_printf_P(PSTR("pin=%u\n"), pin);
        _interruptHandlers.erase(std::remove_if(_interruptHandlers.begin(), _interruptHandlers.end(), [pin](const InterruptHandler &handler) {
            return (handler._pin == pin);
        }));
    }

    static void dump(Print &output) {
        for(auto &handler: _interruptHandlers) {
            handler.dumpHandler(output);
        }
    }

    static InterruptHandlerVector _interruptHandlers;
};

InterruptHandler::InterruptHandlerVector InterruptHandler::_interruptHandlers;

PinValue_t _pinValues[MAX_PINS];

extern "C" {

    // void __real_pinMode(uint8_t, uint8_t);
    // void __real_digitalWrite(uint8_t, uint8_t);
    int __real_digitalRead(uint8_t);
    // int __real_analogRead(uint8_t);
    // void __real_analogWrite(uint8_t, int);
    // void __real_attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode);
    // void __real_attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode);
    // void __real_detachInterrupt(uint8_t pin);

#if 0
    void ICACHE_RAM_ATTR __wrap_pinMode(uint8_t pin, uint8_t mode)
    {
        auto debugPin = VirtualPinDebugger::getPin(pin);
        if (debugPin) {
            debugPin->pinMode(static_cast<VirtualPinMode::BIT>(mode));
        }
        else {
            __real_pinMode(pin, mode);
        }
    }
#endif

#if 0
    void ICACHE_RAM_ATTR __wrap_digitalWrite(uint8_t pin, uint8_t value)
    {
        // auto debugPin = VirtualPinDebugger::getPin(pin);
        // if (debugPin) {
        //     debugPin->digitalWrite(value);
        // }
        // else {
        //     __real_digitalWrite(pin, value);
        // }
        if (pin < MAX_PINS && _pinValues[pin]._hasWrite) {
            _pinValues[pin]._writeValue = value;
        }
        else {
            __real_digitalWrite(pin, value);
        }
    }
#endif

#if 1

    int ICACHE_RAM_ATTR __wrap_digitalRead(uint8_t pin)
    {
        // auto debugPin = VirtualPinDebugger::getPin(pin);
        // if (debugPin) {
        //     return debugPin->digitalRead();
        // }
        // else {
        //     return __real_digitalRead(pin);
        // }
        if (pin < MAX_PINS && _pinValues[pin]._hasRead) {
            return _pinValues[pin]._readValue;
        }
        else {
            return __real_digitalRead(pin);
        }
    }
#endif

#if 0

    int ICACHE_RAM_ATTR __wrap_analogRead(uint8_t pin)
    {
        // auto debugPin = VirtualPinDebugger::getPin(pin);
        // if (debugPin) {
        //     return debugPin->analogRead();
        // }
        // else {
        //     return __real_analogRead(pin);
        // }
        if (pin < MAX_PINS && _pinValues[pin]._hasRead) {
            return _pinValues[pin]._readValue;
        }
        else {
            return __real_analogRead(pin);
        }
    }
#endif

#if 0

    void ICACHE_RAM_ATTR __wrap_analogWrite(uint8_t pin, int value)
    {
        // auto debugPin = VirtualPinDebugger::getPin(pin);
        // if (debugPin) {
        //     debugPin->analogWrite(value);
        // }
        // else {
        //     __real_analogWrite(pin, value);
        // }
        if (pin < MAX_PINS && _pinValues[pin]._hasWrite) {
            _pinValues[pin]._writeValue = value;
        }
        else {
            __real_analogWrite(pin, value);
        }
    }
#endif

#if 0

    void __wrap_attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode)
    {
        InterruptHandler::addHandler(InterruptHandler(pin, (void *)&intRoutine, InterruptHandler::FuncType::FUNC, mode));
        __real_attachInterrupt(pin, intRoutine, mode);
    }

    void __wrap_attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode)
    {
        InterruptHandler::addHandler(InterruptHandler(pin, (void *)&scheduledIntRoutine, InterruptHandler::FuncType::SCHEDULED, mode));
        __real_attachScheduledInterrupt(pin, scheduledIntRoutine, mode);
    }

    void __wrap_detachInterrupt(uint8_t pin)
    {
        InterruptHandler::removeHandler(pin);
        __real_detachInterrupt(pin);
    }
#endif

}

VirtualPinDebugger *VirtualPinDebugger::_instance = nullptr;

const __FlashStringHelper *VirtualPinDebug::getPinClass() const
{
    return FPSTR(PSTR("DBG"));
}

void VirtualPinDebug::digitalWrite(uint8_t value)
{
    if (_pinValues[_pin]._hasWrite) {
        _pinValues[_pin]._writeValue = value;
    }
    else {
        ::digitalWrite(_pin, value);
    }
}

uint8_t VirtualPinDebug::digitalRead() const
{
    if (_pinValues[_pin]._hasRead) {
        return _pinValues[_pin]._readValue;
    }
    // if (_hasValue) {
    //     return _value;
    // }
    return ::digitalRead(_pin);
}

void VirtualPinDebug::pinMode(VirtualPinMode mode)
{
    ::pinMode(_pin, mode);
}

bool VirtualPinDebug::isAnalog() const
{
    return (_pin == PIN_A0);
}

void VirtualPinDebug::analogWrite(int32_t value)
{
    if (_pinValues[_pin]._hasWrite) {
        _pinValues[_pin]._writeValue = value;
    }
    else {
        ::analogWrite(_pin, value);
    }
}

int32_t VirtualPinDebug::analogRead() const
{
    if (_pinValues[_pin]._hasRead) {
        return _pinValues[_pin]._readValue;
    }
    // if (_hasValue) {
    //     return _value;
    // }
    return ::analogRead(_pin);
}

uint8_t VirtualPinDebug::getPin() const
{
    return _pin;
}

void VirtualPinDebug::setReadValue(int value)
{
    __setReadValue(value);
    // _value = value;
    // _hasValue = true;
}

class TimerSequence {
public:
    TimerSequence(VirtualPinDebug &pin, const VirtualPinDebug::SequenceVector::const_iterator &begin, const VirtualPinDebug::SequenceVector::const_iterator &end) : _pin(pin) {
        _sequence.assign(begin, end);
        _iterator = _sequence.begin();
    }

    void timerCallback(EventScheduler::TimerPtr timer) {
        _debug_printf("left %u value %u timeout %u\n", std::distance(_iterator, _sequence.end()), _iterator->_value, _iterator->_timeout);
        int timeout = _iterator->_timeout;
        _pinValues[_pin]._readValue = _iterator->_value;
        // _pin._value  = _iterator->_value;
        if (++_iterator == _sequence.end()) {
            timer->detach();
        }
        else {
            timer->rearm(timeout, true);
        }
    }

private:
    VirtualPinDebug &_pin;
    VirtualPinDebug::SequenceVector::iterator _iterator;
    VirtualPinDebug::SequenceVector _sequence;
};

void VirtualPinDebug::setReadValueSequence(const SequenceVector &sequence)
{
    _debug_printf_P(PSTR("size=%u\n"), sequence.size());
    if (sequence.size() < 2) {
        return;
    }

    _timer.remove();

    auto iterator = sequence.begin();
    int timeout = iterator->_timeout;
    setReadValue(iterator->_value);

    TimerSequence *seq = new TimerSequence(*this, ++iterator, sequence.end());

    _timer.add(timeout, true, [seq](EventScheduler::TimerPtr timer) {
        seq->timerCallback(timer);
    }, EventScheduler::PRIO_NORMAL, [seq](EventTimer *ptr) {
        debug_printf("deleter called %p seq %p\n", ptr, seq);
        delete seq;
        delete ptr;
    });
}

uint32_t VirtualPinDebug::getTimeout() const
{
    if (_timer.active()) {
        return _timer->getDelay();
    }
    return 0;
}


void VirtualPinDebugger::addPin(VirtualPinDebugPtr pin)
{
    _debug_printf_P(PSTR("pin=%u\n"), pin->getPin());
    getInstance()._pins.emplace_back(pin);
}

void VirtualPinDebugger::removePin(VirtualPinDebugPtr pin)
{
    _debug_printf_P(PSTR("pin=%u\n"), pin->getPin());
    auto &pins = getInstance()._pins;
    pins.erase(std::remove(pins.begin(), pins.end(), pin));
}

void VirtualPinDebugger::removeAll()
{
    _debug_printf_P(PSTR("instance=%p\n"), _instance);
    if (_instance) {
        getInstance()._pins.clear();
    }
}

#endif
