/**
* Author: sascha_lammers@gmx.de
*/

#ifndef ESP32

#pragma once

#include <Arduino_compat.h>
#include "VirtualPin.h"

class VirtualPinGPIO : public VirtualPin
{
public:
    VirtualPinGPIO();
    VirtualPinGPIO(uint8_t pin);

    virtual void digitalWrite(uint8_t value);
    virtual uint8_t digitalRead() const;
    virtual void pinMode(VirtualPinMode mode);
    virtual bool isAnalog() const;
    virtual void analogWrite(int value);
    virtual int analogRead() const;
    virtual uint8_t getPin() const;
    virtual const __FlashStringHelper *getPinClass() const;

private:
    uint8_t _pin;
};

#endif
