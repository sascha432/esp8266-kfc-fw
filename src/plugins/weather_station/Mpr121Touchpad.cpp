/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include "Mpr121Touchpad.h"

volatile bool mpr121_irq_callback_flag;

void ICACHE_RAM_ATTR mpr121_irq_callback()
{
    mpr121_irq_callback_flag = true;
}

Mpr121Touchpad::Mpr121Touchpad()  : _irqPin(0)
{
}

Mpr121Touchpad::~Mpr121Touchpad()
{
    end();
}

bool Mpr121Touchpad::begin(uint8_t address, uint8_t irqPin, TwoWire *wire)
{
    if (_irqPin) {
        return false;
    }
    if (!_mpr121.begin(address, wire)) {
        return false;
    }

    _irqPin = irqPin;
    mpr121_irq_callback_flag = false;
    pinMode(_irqPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(_irqPin), mpr121_irq_callback, CHANGE);
    _mpr121.setThresholds(4, 4);

    _x = 0;
    _y = 0;
    _get();
    return true;
}

void Mpr121Touchpad::end()
{
    if (_irqPin) {
        detachInterrupt(digitalPinToInterrupt(_irqPin));
        _irqPin = 0;
    }
}

bool Mpr121Touchpad::isEventPending() const
{
    return mpr121_irq_callback_flag;
}

void Mpr121Touchpad::get(uint8_t &x, uint8_t &y)
{
    _get();
    x = _x;
    y = _y;
}

void Mpr121Touchpad::_get()
{
    if (mpr121_irq_callback_flag) {
        mpr121_irq_callback_flag = false;

        auto touched = _mpr121.touched();

        if ((touched & (_BV(11)|_BV(10))) == (_BV(11)|_BV(10))) {
            _y = 2;
        }
        else if (touched & _BV(11)) {
            _y = 1;
        }
        else if ((touched & (_BV(10)|_BV(9))) == (_BV(10)|_BV(9))) {
            _y = 4;
        }
        else if (touched & _BV(10)) {
            _y = 3;
        }
        else if ((touched & (_BV(9)|_BV(8))) == (_BV(9)|_BV(8))) {
            _y = 6;
        }
        else if (touched & (_BV(9))) {
            _y = 5;
        }
        else if (touched & (_BV(8))) {
            _y = 7;
        }
        else {
            _y = 0;
        }

        if ((touched & (_BV(7)|_BV(6))) == (_BV(7)|_BV(6))) {
            _x = 2;
        }
        else if (touched & _BV(7)) {
            _x = 1;
        }
        else if ((touched & (_BV(6)|_BV(5))) == (_BV(6)|_BV(5))) {
            _x = 4;
        }
        else if (touched & _BV(6)) {
            _x = 3;
        }
        else if ((touched & (_BV(5)|_BV(4))) == (_BV(5)|_BV(4))) {
            _x = 6;
        }
        else if (touched & _BV(5)) {
            _x = 5;
        }
        else if ((touched & (_BV(4)|_BV(3))) == (_BV(4)|_BV(3))) {
            _x = 8;
        }
        else if (touched & _BV(4)) {
            _x = 7;
        }
        else if ((touched & (_BV(3)|_BV(2))) == (_BV(3)|_BV(2))) {
            _x = 10;
        }
        else if (touched & _BV(3)) {
            _x = 9;
        }
        else if ((touched & (_BV(2)|_BV(1))) == (_BV(2)|_BV(1))) {
            _x = 12;
        }
        else if (touched & _BV(2)) {
            _x = 11;
        }
        else if (touched & _BV(1)) {
            _x = 13;
        }
        else {
            _x = 0;
        }
    }
}

#endif
