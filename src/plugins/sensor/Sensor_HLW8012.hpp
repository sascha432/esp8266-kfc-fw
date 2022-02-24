/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_HLW8012

// #include <Arduino_compat.h>
// #include <PrintString.h>
#include "Sensor_HLW8012.h"

inline Sensor_HLW8012::~Sensor_HLW8012()
{
    LoopFunctions::remove(Sensor_HLW8012::loop);
    sensor = nullptr;
    detachInterrupt(digitalPinToInterrupt(_pinCF));
    detachInterrupt(digitalPinToInterrupt(_pinCF1));
    UNREGISTER_SENSOR_CLIENT(this);
}


inline String Sensor_HLW8012::_getId(const __FlashStringHelper *type) const
{
    PrintString id(F("hlw8012_0x%04x"), (_pinSel << 10) | (_pinCF << 5) | (_pinCF1));
    if (type) {
        id.write('_');
        id.print(FPSTR(type));
    }
    return id;
}

inline void Sensor_HLW8012::loop()
{
    hlwSensor->_loop();
}

inline uint8_t Sensor_HLW8012::_getCFPin() const
{
    return _pinCF;
}

inline uint8_t Sensor_HLW8012::_getCF1Pin() const
{
    return _pinCF1;
}

inline char Sensor_HLW8012::_getOutputMode(SensorInput *input) const
{
    if (input == &_inputCF) {
        return 'P';
    }
    else if (input == &_inputCFU) {
        return 'U';
    }
    else if (input == &_inputCFI) {
        return 'I';
    }
    return '?';
}

#endif
