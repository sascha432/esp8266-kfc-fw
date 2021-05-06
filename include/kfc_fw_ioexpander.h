/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if HAVE_PCF8574
#include <IOExpander.h>
using _PCF8574Range = IOExpander::PCF8574_Range<PCF8574_PORT_RANGE_START, PCF8574_PORT_RANGE_END>;
extern IOExpander::PCF8574 _PCF8574;
// these functions must be implemented and exported
extern void initialize_pcf8574();
extern void print_status_pcf8574(Print &output);
#endif

#if HAVE_PCF8575
#include <PCF8575.h>
extern PCF8575 _PCF8575;
extern void initialize_pcf8575();
extern void print_status_pcf8575(Print &output);
#endif

#if HAVE_PCA9685
extern void initialize_pca9785();
extern void print_status_pca9785(Print &output);
#endif
#if HAVE_MCP23017
extern void initialize_mcp23017();
extern void print_status_mcp23017(Print &output);
#endif

inline static void _digitalWrite(uint8_t pin, uint8_t value) {
#if HAVE_PCF8574

    if (_PCF8574Range::inRange(pin)) {
        _PCF8574.digitalWrite(_PCF8574Range::digitalPin2Pin(pin), value);
    }
    else
#endif
    {
        digitalWrite(pin, value);
    }
}

inline static uint8_t _digitalRead(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return _PCF8574.digitalRead(_PCF8574Range::digitalPin2Pin(pin));
    }
    else
#endif
    {
        return digitalRead(pin);
    }
}

inline static void _analogWrite(uint8_t pin, uint16_t value) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        _PCF8574.digitalWrite(_PCF8574Range::digitalPin2Pin(pin), value ? HIGH : LOW);
    }
    else
#endif
    {
        analogWrite(pin, value);
    }
}

inline static uint16_t _analogRead(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return _PCF8574.digitalRead(_PCF8574Range::digitalPin2Pin(pin)) ? 1023 : 0;
    }
    else
#endif
    {
        return analogRead(pin);
    }

}

inline static void _pinMode(uint8_t pin, uint8_t mode) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        _PCF8574.pinMode(_PCF8574Range::digitalPin2Pin(pin), mode);
    }
    else
#endif
    {
        pinMode(pin, mode);
    }
}

inline static size_t _pinName(uint8_t pin, char *buf, size_t size) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return snprintf_P(buf, size - 1, PSTR("%u (PCF8574 pin %u)"), pin, _PCF8574Range::digitalPin2Pin(pin));
    }
    else
#endif
    {
        return snprintf_P(buf, size - 1, PSTR("%u"), pin);
    }
}

inline static bool _pinHasAnalogRead(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return false;
    }
    else
#endif
    {
#if ESP8266
        return (pin == A0);
#else
#error missing
#endif
    }

}

inline static bool _pinHasAnalogWrite(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return false;
    }
    else
#endif
    {
#if ESP8266
        return pin < A0;
#else
#error missing
#endif
    }

}


inline  __attribute__((__always_inline__))
void BlinkLEDTimer::_digitalWrite(uint8_t pin, uint8_t value) {
    ::_digitalWrite(pin, value);
}

inline  __attribute__((__always_inline__))
void BlinkLEDTimer::_digitalWrite(uint8_t value) {
    ::_digitalWrite(_pin, value);
}
