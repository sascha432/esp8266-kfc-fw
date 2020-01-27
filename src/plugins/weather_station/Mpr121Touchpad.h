/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include <Adafruit_MPR121.h>

class Mpr121Touchpad {
public:
    Mpr121Touchpad();
    ~Mpr121Touchpad();

    bool begin(uint8_t address, uint8_t irqPin, TwoWire *wire = &Wire);
    void end();

    // returns true until get() has been called to retrieve the updated coordinates
    bool isEventPending() const;

    void processEvents();

    // retrieve coordinates
    void get(uint8_t &x, uint8_t &y);

private:
    void _get();

private:
    Adafruit_MPR121 _mpr121;
    uint8_t _address;
    uint8_t _irqPin;
    uint8_t _x, _y;
    double _moveXInt;
    double _moveYInt;
    uint32_t _eventTime;
    uint32_t _touchedTime;
    uint32_t _releasedTime;
};

#endif
