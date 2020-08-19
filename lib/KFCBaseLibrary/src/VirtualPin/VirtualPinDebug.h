/**
* Author: sascha_lammers@gmx.de
*/

#ifndef ESP32

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <EventScheduler.h>
#include "VirtualPin.h"

#if DEBUG_VIRTUAL_PIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class PinValue_t {
public:
    PinValue_t() : _hasRead(0), _hasWrite(0) {
    }
    int32_t _readValue;
    int32_t _writeValue;
    uint8_t _hasRead: 1;
    uint8_t _hasWrite: 1;
};

#define MAX_PINS 17

extern PinValue_t _pinValues[MAX_PINS];

class TimerSequence;

class VirtualPinDebug : public VirtualPin
{
public:
    class Sequence {
    public:
        Sequence(int value, int timeout) : _value(value), _timeout(timeout) {
        }
        int _value;
        int _timeout;
    };
    typedef std::vector<Sequence> SequenceVector;

    VirtualPinDebug(uint8_t pin = 0) : _pin(pin)/*, _value(0), _hasValue(false)*/ {
    }
    ~VirtualPinDebug() {
        _timer.remove();
    }

    virtual void digitalWrite(uint8_t value);
    virtual uint8_t digitalRead() const;
    virtual void pinMode(VirtualPinMode mode);
    virtual bool isAnalog() const;
    virtual void analogWrite(int value);
    virtual int analogRead() const;
    virtual uint8_t getPin() const;
    virtual const __FlashStringHelper *getPinClass() const;

    // set value that analog/digitalRead return
    void setReadValue(int value);
    // return value by reading port
    void removeReadValue() {
        _timer.remove();
        // _hasValue = false;
        _pinValues[_pin]._hasRead = false;
    }
    bool hasReadValue() const {
        return _pinValues[_pin]._hasRead; //_hasValue;
    }
    int getReadValue() const {
        return _pinValues[_pin]._readValue; //_value;
    }
    void setReadValueSequence(const SequenceVector &sequence);

    uint32_t getTimeout() const;

    inline void __setReadValue(int value) {
        _pinValues[_pin]._readValue = value;
        _pinValues[_pin]._hasRead = true;
        // _value = value;
        // _hasValue = true;
    }

private:
    friend TimerSequence;
    uint8_t _pin;
    // int _value;
    // bool _hasValue;
    Event::Timer _timer;
};


class VirtualPinDebugger
{
private:
    VirtualPinDebugger() {
    }
public:
    typedef VirtualPinDebug *VirtualPinDebugPtr;
    typedef std::vector<VirtualPinDebugPtr> VirtualPinDebugVector;

    ~VirtualPinDebugger() = delete;

    static size_t getPinCount() {
        _debug_printf_P(PSTR("instance=%p pins=%u\n"), _instance, _instance ? getInstance()._pins.size() : 0);
        if (_instance) {
            return getInstance()._pins.size();
        }
        return 0;
    }

    const VirtualPinDebugVector &getPins() const {
        return _pins;
    }

public:
    static VirtualPinDebugger &getInstance() {
        if (_instance == nullptr) {
            return *(_instance = new VirtualPinDebugger());
        }
        return *_instance;
    }

    static VirtualPinDebugPtr getPin(uint8_t pin) {
        if (_instance) {
            auto &pins = _instance->_pins;
            auto iterator = std::find_if(pins.begin(), pins.end(), [pin](VirtualPinDebugPtr debugPin) {
                return debugPin->getPin() == pin;
            });
            if (iterator != pins.end()) {
                return *iterator;
            }
        }
        return nullptr;
    }

    static void addPin(VirtualPinDebugPtr pin);
    static void removePin(VirtualPinDebugPtr pin);
    static void removeAll();

private:
    VirtualPinDebugVector _pins;

private:
    static VirtualPinDebugger *_instance;
};

#include <debug_helper_disable.h>

#endif
