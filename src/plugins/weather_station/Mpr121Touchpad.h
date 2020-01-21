/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Adafruit_MPR121.h>

class Mpr121Touchpad {
public:
    Mpr121Touchpad();
    ~Mpr121Touchpad();

    bool begin(uint8_t address, uint8_t irqPin, TwoWire *wire = &Wire);
    void end();

    // returns true until get() has been called to retrieve the updated coordinates
    bool isEventPending() const;

    // retrieve coordinates
    void get(uint8_t &x, uint8_t &y);

private:
    void _get();

private:
    Adafruit_MPR121 _mpr121;
    uint8_t _irqPin;
    uint8_t _x, _y;
};

