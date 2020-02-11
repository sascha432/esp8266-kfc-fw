/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// abstract class for digital and analog IO ports

#include <Arduino_compat.h>
#include "EnumBitset.h"

#ifndef DEBUG_VIRTUAL_PIN
#define DEBUG_VIRTUAL_PIN                                   1
#endif

#if ESP8266

#undef INPUT
#undef INPUT_PULLUP
#undef INPUT_PULLDOWN_16
#undef OUTPUT
#undef OUTPUT_OPEN_DRAIN
#undef WAKEUP_PULLUP
#undef WAKEUP_PULLDOWN
#undef SPECIAL
#undef BIT

DECLARE_ENUM_BITSET(VirtualPinMode, uint8_t,
    INPUT             = 0x00,
    INPUT_PULLUP      = 0x02,
    INPUT_PULLDOWN_16 = 0x04,
    OUTPUT            = 0x01,
    OUTPUT_OPEN_DRAIN = 0x03,
    WAKEUP_PULLUP     = 0x05,
    WAKEUP_PULLDOWN   = 0x07,
    SPECIAL           = 0xF8
);

#else

DECLARE_ENUM(VirtualPinMode, uint8_t,
    INPUT             = 0x00,
    OUTPUT            = 0x01,
);

#endif

class VirtualPin
{
public:
    // set pin value
    virtual void digitalWrite(uint8_t value) = 0;

    // read pin value
    virtual uint8_t digitalRead() const = 0;

    // set pin mode
    virtual void pinMode(VirtualPinMode mode) = 0;

    // returns true if analogRead is supported
    virtual bool isAnalog() const {
        return false;
    }

    // write value
    virtual void analogWrite(int value) {
        digitalWrite(value);
    }

    // read value
    virtual int analogRead() const {
        return digitalRead();
    }

    // return pin
    virtual uint8_t getPin() const = 0;

    // get class of pin
    virtual const __FlashStringHelper *getPinClass() const = 0;

public:

    bool operator == (const VirtualPin &pin) const {
        return (getPin() == pin.getPin()) && (getPinClass() == pin.getPinClass());
    }
    bool operator != (const VirtualPin &pin) const {
        return !(*this == pin);
    }

    bool operator == (uint8_t pin) const {
        return (getPin() == pin) && strcmp_P_P(reinterpret_cast<PGM_P>(getPinClass()), PSTR("GPIO")) == 0;
    }
    bool operator != (uint8_t pin) const {
        return !(*this == pin);
    }

    operator uint8_t() const {
        return getPin();
    }
};
