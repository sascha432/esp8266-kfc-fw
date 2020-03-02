/**
  Author: sascha_lammers@gmx.de
*/

// read ADC and provide more stable results
// special algorithm for ESP8266, cannot be used if the ADC readings change quickly

#pragma once

#include <Arduino_compat.h>
#include "FixedCircularBuffer.h"

class ReadADC {
public:
    ReadADC(uint8_t pin = A0, uint8_t num = 3, uint8_t delay = 5) : _pin(pin), _num(num), _delay(delay) {
        init();
    }

#if ESP8266
    void init() {
        for(uint8_t i = 0; i < 32; i++) {
            _buffer.push_back(analogRead(_pin));
            delay(1);
        }
    }

    uint16_t read() {
        uint32_t sum = 0;
        uint8_t numValues  = 0;
        uint16_t min, max;
        for(uint8_t i = 0; i < _num * 10; i++) {
            uint16_t value = analogRead(_pin);
            _get(min, max);
            // debug_printf_P(PSTR("readadc(): value=%u, %u/%u=%u\n"), value, min, max, (value >= min && value <= max));
            if (value >= min && value <= max) {
                sum += value;
                i += 9;
                numValues++;
            }
            _buffer.push_back(value);
            if (_delay) {
                delay(_delay);
            }
        }
        if (numValues == 0) {
            uint16_t value = analogRead(_pin);
            _buffer.push_back(value);
            // debug_printf_P(PSTR("readadc(): result=%u/0\n"), value);
            return value;
        }
        // debug_printf_P(PSTR("readadc(): result=%u/%u\n"), sum / numValues, numValues);
        return sum / numValues;
    }

private:
    void _get(uint16_t &min, uint16_t &max) {
        uint32_t mean = 0;
        for(auto value: _buffer) {
            mean += value;
        }
        mean /= _buffer.size();
        min = mean * 1000 / 1004;
        max = mean * 1000 / 985;
    }

private:
    FixedCircularBuffer<uint16_t, 32> _buffer;

#else

    void init() {
    }

    uint16_t read() {
        uint32_t sum = 0;
        for(uint8_t i = 0; i < _num; i++) {
            sum += analogRead(_pin);
            if (_delay) {
                delay(_delay);
            }
        }
        return sum / _num;
    }
#endif

private:
    uint8_t _pin;
    uint8_t _num;
    uint8_t _delay;
};
