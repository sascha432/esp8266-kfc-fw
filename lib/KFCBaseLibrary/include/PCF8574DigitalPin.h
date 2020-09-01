/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "DigitalPin.h"
#include <PCF8574.h>

#ifndef PCF8574_DIGITAL_PIN_DEBUG
#define PCF8574_DIGITAL_PIN_DEBUG 0
#endif

#if PCF8574_DIGITAL_PIN_DEBUG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class PCF8574DigitalPin : public DigitalPin {
public:
    PCF8574DigitalPin(PCF8574 &pcf8574, uint8_t pin) : DigitalPin(pin), _pcf8574(pcf8574) {
    }

    PCF8574DigitalPin(PCF8574 &pcf8574, uint8_t pin, uint8_t mode) : PCF8574DigitalPin(pcf8574, pin) {
        pinMode(mode);
    }

    virtual void digitalWrite(uint8_t value) {
        __LDBG_printf("pin=%u,value=%u", _pin, value);
        _pcf8574.digitalWrite(_pin, value);
        _debug_println(F("done");
    }

    virtual bool digitalRead() {
#if PCF8574_DIGITAL_PIN_DEBUG
        __LDBG_printf("pin=%u", _pin);
        auto result = _pcf8574.digitalRead(_pin);
        __LDBG_printf("result=%u", result);
        return result;
#else
        return _pcf8574.digitalRead(_pin);
#endif
    }

    virtual void pinMode(uint8_t mode) {
        __LDBG_printf("=%u,mode=%u", _pin, mode);
        _pcf8574.pinMode(_pin, mode);
        _debug_println(F("done");
    }

    virtual uint8_t getPinClass() const {
        return 1;
    }

private:
    PCF8574 &_pcf8574;
};

#if PCF8574_DIGITAL_PIN_DEBUG
#include <debug_helper_disable.h>
#endif
