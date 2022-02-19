/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"

inline BlindsControl::ToneSettings::ToneSettings(uint16_t _frequency, uint16_t _pwmValue, uint8_t _pins[2], uint32_t _timeout)  :
    counter(0),
    loop(0),
    runtime(_timeout ? get_time_diff(millis(), _timeout) : 0),
    frequency(_frequency),
    pwmValue(_pwmValue),
    interval(kToneInterval),
    pin({_pins[0], _pins[1]})
{
    __LDBG_assert_panic(!(_pins[0] == kInvalidPin && _pins[1] != kInvalidPin), "pin1=%u not set, pin2=%u set", _pins[0], _pins[1]);
}

inline bool BlindsControl::ToneSettings::hasPin(uint8_t num) const
{
    return (num < pin.size()) && (pin[num] != kInvalidPin);
}

// returns if first pin is available
// if the first pin is invalid, the second pin is invalid as well
inline bool BlindsControl::ToneSettings::hasPin1() const
{
    return pin[0] != kInvalidPin;
}

// returns true if second pin is available
inline bool BlindsControl::ToneSettings::hasPin2() const
{
    return pin[1] != kInvalidPin;
}

inline void BlindsControl::_playTone(uint8_t *pins, uint16_t pwm, uint32_t frequency)
{
    if (*pins == kInvalidPin) {
        return;
    }
    __LDBG_printf("tone pins=%u,%u pwm=%u freq=%u", pins[0], pins[1], pwm, frequency);
    analogWriteFreq(frequency);
    analogWrite(pins[0], pwm);
    #if !defined(ESP8266)
        if (pins[1] != kInvalidPin)
    #endif
    {
        analogWrite(pins[1], pwm);
    }
}

inline void BlindsControl::_playNote(uint8_t pin, uint16_t pwm, uint8_t note)
{
    uint8_t tmp = note + IMPERIAL_MARCH_NOTE_OFFSET;
    if (tmp < NOTE_TO_FREQUENCY_COUNT) {
        uint8_t pins[2] = { pin, kInvalidPin };
        _playTone(pins, pwm, NOTE_FP_TO_INT(pgm_read_word(note_to_frequency + tmp)));
    }
}

inline void BlindsControl::_stopToneTimer()
{
    __LDBG_printf("stopping tone timer and disabling motors");
    _toneTimer.remove();
    _disableMotors();
}
