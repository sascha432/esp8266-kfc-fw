/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "kfc_fw_ioexpander.h"

// digitalWrite and digitalRead
// the number of clock cycles stays the same for GPIO pins

extern void IRAM_ATTR digitalWrite(uint8_t pin, uint8_t val)
{
    stopWaveform(pin); // Disable any Tone or startWaveform on this pin
    _stopPWM(pin); // and any analogWrites (PWM)
    if (pin < 16) {
        if (val) {
            GPOS = (1 << pin);
        }
        else {
            GPOC = (1 << pin);
        }
    } else if (pin == 16) {
        if (val) {
            GP16O |= 1;
        }
        else {
            GP16O &= ~1;
        }
    }
    else {
        IOExpander::config.digitalWrite(pin, val);
    }
}

extern int IRAM_ATTR digitalRead(uint8_t pin)
{
    if (pin < 16) {
        return GPIP(pin);
    }
    else if (pin == 16) {
        return GP16I & 0x01;
    }
    return IOExpander::config.digitalRead(pin);
}

extern void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin >= 16) {
        IOExpander::config.pinMode(pin, mode);
        return;
    }
    __pinMode(pin, mode);
}


extern int analogRead(uint8_t pin)
{
    if (pin == A0) {
        return analogRead(pin);
    }
    return IOExpander::config.analogRead(pin);
}

extern void analogReference(uint8_t mode)
{
    __analogReference(mode);
    IOExpander::config.analogReference(mode);
}

extern void analogWrite(uint8_t pin, int val)
{
    if (pin == A0) {
        __analogWrite(pin, val);
        return;
    }
    IOExpander::config.analogWrite(pin, val);
}

extern void analogWriteFreq(uint32_t freq)
{
    __analogWriteFreq(freq);
    IOExpander::config.analogWriteFreq(freq);
}
