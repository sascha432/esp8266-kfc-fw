/**
* Author: sascha_lammers@gmx.de
*/

#include "VirtualPinGPIO.h"

#if DEBUG_VIRTUAL_PIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

VirtualPinGPIO::VirtualPinGPIO() : _pin(0)
{
}

VirtualPinGPIO::VirtualPinGPIO(uint8_t pin) : _pin(pin)
{
}

const __FlashStringHelper *VirtualPinGPIO::getPinClass() const
{
    return FPSTR(PSTR("GPIO"));
}

void VirtualPinGPIO::digitalWrite(uint8_t value)
{
    ::digitalWrite(_pin, value);
}

uint8_t VirtualPinGPIO::digitalRead() const
{
    return ::digitalRead(_pin);
}

void VirtualPinGPIO::pinMode(VirtualPinMode mode)
{
    ::pinMode(_pin, static_cast<uint8_t>(mode));
}

bool VirtualPinGPIO::isAnalog() const
{
    return _pin == PIN_A0;
}

void VirtualPinGPIO::analogWrite(int32_t value)
{
    ::analogWrite(_pin, value);
}

int32_t VirtualPinGPIO::analogRead() const
{
    if (_pin == PIN_A0) {
        return ::analogRead(_pin);
    }
    return digitalRead();
}

uint8_t VirtualPinGPIO::getPin() const
{
    return _pin;
}

