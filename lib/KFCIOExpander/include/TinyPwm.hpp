/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"

namespace IOExpander {

    // ------------------------------------------------------------------
    // TinyPwm
    // ------------------------------------------------------------------

    namespace TinyPwmNS {

        enum class Commands : uint8_t {
            ANALOG_READ = 0x11,
            ANALOG_WRITE = 0x12,
            DIGITAL_READ = 0x13,
            DIGITAL_WRITE = 0x14,
            PIN_MODE = 0x15,
            WRITE_EEPROM = 0x41,
            RESET_EEPROM = 0x42,
            SET_PWM_FREQUENCY = 0x51,
            ADC_SET_AREF = 0x62,
            ADC_SET_READ_CYCLES = 0x63,
        };

        struct Command {
            union {
                struct {
                    Commands command;
                    union {
                        struct {
                            uint16_t frequency;
                        } SET_PWM_FREQUENCY;
                        struct {
                            uint8_t pin;            // internal pin number
                            uint8_t pwmValue;
                        } ANALOG_WRITE;
                        struct {
                            uint8_t pin;            // internal pin number
                        } ANALOG_READ;
                        struct {
                            uint8_t pin;            // internal pin number
                        } DIGITAL_READ;
                        struct {
                            uint8_t pin;            // internal pin number
                            uint8_t value;
                        } DIGITAL_WRITE;
                        struct {
                            uint8_t pin;            // internal pin number
                            uint8_t mode;
                        } PIN_MODE;
                        struct {
                            uint8_t mode;
                        } ADC_SET_AREF;
                        struct {
                            uint8_t pin;
                            uint8_t cycles;
                        } ADC_SET_READ_CYCLES;
                    };
                };
            };
            operator uint8_t *() {
                return reinterpret_cast<uint8_t *>(this);
            }
            operator const uint8_t *() const {
                return reinterpret_cast<const uint8_t *>(this);
            }
        };

    };

    IOEXPANDER_INLINE int TinyPwm::analogRead(uint8_t pin)
    {
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::ANALOG_READ};
        command.ANALOG_READ.pin = pin;
        _wire->write(command, sizeof(command.ANALOG_READ) + 1);
        if (_wire->endTransmission(true) == 0 && _wire->available()) {
            int16_t value;
            if (_wire->readBytes(reinterpret_cast<uint8_t *>(&value), sizeof(value)) == sizeof(value)) {
                return value;
            }
        }
        return 0;
    }

    IOEXPANDER_INLINE uint8_t TinyPwm::digitalRead(uint8_t pin)
    {
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::DIGITAL_READ};
        command.DIGITAL_READ.pin = pin;
        _wire->write(command, sizeof(command.DIGITAL_READ) + 1);
        if (_wire->endTransmission(true) == 0 && _wire->available()) {
            uint8_t value;
            if (_wire->readBytes(reinterpret_cast<uint8_t *>(&value), sizeof(value)) == sizeof(value)) {
                return value;
            }
        }
        return 0;
    }

    IOEXPANDER_INLINE bool TinyPwm::analogWrite(uint8_t pin, uint8_t value)
    {
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::ANALOG_WRITE};
        command.ANALOG_WRITE.pin = pin;
        command.ANALOG_WRITE.pwmValue = value;
        _wire->write(command, sizeof(command.ANALOG_WRITE) + 1);
        if (_wire->endTransmission(true) != 0) {
            __LDBG_printf("endTransmission() failed");
            return false;
        }
        return true;
    }

    IOEXPANDER_INLINE void TinyPwm::analogWriteFreq(uint16_t frequency)
    {
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::SET_PWM_FREQUENCY};
        command.SET_PWM_FREQUENCY.frequency = frequency;
        _wire->write(command, sizeof(command.SET_PWM_FREQUENCY) + 1);
        if (_wire->endTransmission(true) != 0) {
            __LDBG_printf("endTransmission() failed");
        }
    }

    IOEXPANDER_INLINE void TinyPwm::analogReference(uint8_t mode)
    {
        if (mode != INTERNAL_1V1 && mode != INTERNAL_2V56) {
            return;
        }
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::ADC_SET_AREF};
        command.ADC_SET_AREF.mode = mode & ~0x80;
        _wire->write(command, sizeof(command.ADC_SET_AREF) + 1);
        if (_wire->endTransmission(true) != 0) {
            __LDBG_printf("endTransmission() failed");
        }
    }

    IOEXPANDER_INLINE void TinyPwm::digitalWrite(uint8_t pin, uint8_t value)
    {
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::DIGITAL_WRITE};
        command.DIGITAL_WRITE.pin = pin;
        command.DIGITAL_WRITE.value = value;
        _wire->write(command, sizeof(command.DIGITAL_WRITE) + 1);
        if (_wire->endTransmission(true) != 0) {
            __LDBG_printf("endTransmission() failed");
        }
    }

    IOEXPANDER_INLINE void TinyPwm::pinMode(uint8_t pin, uint8_t mode)
    {
        _wire->beginTransmission(_address);
        TinyPwmNS::Command command = {TinyPwmNS::Commands::PIN_MODE};
        command.PIN_MODE.pin = pin;
        command.PIN_MODE.mode = mode;
        _wire->write(command, sizeof(command.PIN_MODE) + 1);
        if (_wire->endTransmission(true) != 0) {
            __LDBG_printf("endTransmission() failed");
        }
    }

}

