/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#ifndef DIGITAL_PIN_DEBUG
#define DIGITAL_PIN_DEBUG 0
#endif

#if DIGITAL_PIN_DEBUG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class DigitalPin {
public:
    DigitalPin() : _pin(INVALID_PIN) {
    }

    DigitalPin(uint8_t pin) : _pin(pin) {
    }

    virtual ~DigitalPin() {
    }

    virtual void digitalWrite(uint8_t value) {
        _debug_printf("pin=%u,value=%u\n", _pin, value);
        ::digitalWrite(_pin, value);
        _debug_printf("done\n");
    }

    virtual bool digitalRead() {
#if DIGITAL_PIN_DEBUG
        _debug_printf("pin=%u\n", _pin);
        auto result = ::digitalRead(_pin);
        _debug_printf("result=%u\n", result);
        return result;
#else
        return ::digitalRead(_pin);
#endif
    }

    virtual void pinMode(uint8_t mode) {
        _debug_printf("pin=%u,mode=%u\n", _pin, mode);
        ::pinMode(_pin, mode);
        _debug_printf("done\n");
    }

    // must be unique per class to identify pins with the same value
    virtual uint8_t getPinClass() const {
        return 0;
    }

    bool isValid() const {
        return _pin != INVALID_PIN;
    }

    uint8_t getPin() const {
        return _pin;
    }

    bool operator == (const DigitalPin &dp) const {
        return _pin == dp._pin && getPinClass() == dp.getPinClass();
    }

    bool operator != (const DigitalPin &dp) const {
        return _pin != dp._pin ||  getPinClass() != dp.getPinClass();
    }

protected:
    const uint8_t INVALID_PIN = 0xff;

    uint8_t _pin;
};

#if DIGITAL_PIN_DEBUG
#include <debug_helper_disable.h>
#endif
