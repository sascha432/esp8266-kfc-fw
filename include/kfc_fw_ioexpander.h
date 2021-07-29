/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if HAVE_PCF8574 || HAVE_TINYPWM || HAVE_MCP23017 || HAVE_PCA9685 || HAVE_IOEXPANDER

#include <IOExpander.h>

#ifdef ESP8266

#include <core_esp8266_waveform.h>
#include <interrupts.h>
// #include <FunctionalInterrupt.h>

extern "C" {

    void pinMode(uint8_t pin, uint8_t mode);
    void digitalWrite(uint8_t pin, uint8_t val);
    int digitalRead(uint8_t pin);
    int analogRead(uint8_t pin);

    int analogRead(uint8_t pin);
    void analogReference(uint8_t mode);
    void analogWrite(uint8_t pin, int val);
    void analogWriteFreq(uint32_t freq);

    void __pinMode(uint8_t pin, uint8_t mode);
    int __analogRead(uint8_t pin);
    void __analogReference(uint8_t mode);
    void __analogWrite(uint8_t pin, int val);
    void __analogWriteFreq(uint32_t freq);

}

#else

#error platform not supported

#endif

#endif
