/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"
#include "debug_helper_disable.h"

#ifndef IOEXPANDER_INLINE
#define IOEXPANDER_INLINE inline
#endif

// DDR high = output, low = input. PORT is unchanged
// PIN reads from the exapnder
// PORT writes to the expander if output it enabled

namespace IOExpander {

    namespace IOWrapper {

        // ------------------------------------------------------------------
        // PCF8574_PIN
        // ------------------------------------------------------------------

        IOEXPANDER_INLINE PCF8574_PIN::operator uint8_t()
        {
            return _readValue();
        }

        IOEXPANDER_INLINE uint8_t PCF8574_PIN::_readValue()
        {
            if (_parent->hasWire()) {
                auto &wire = _parent->getWire();
                if (wire.requestFrom(_parent->getAddress(), (uint8_t)0x01) == 1) {
                    int data;
                    while(wire.available() && (data = wire.read()) != -1) {
                        _value = data;
                    }
                }
            }
            // __DBG_printf("read PIN=%02x DDR=%02x PORT=%02x _value=%02x", _parent->PIN._value, _parent->DDR._value, _parent->PORT._value, _value);
            return _value;
        }

        // ------------------------------------------------------------------
        // PCF8574_PORT
        // ------------------------------------------------------------------

        IOEXPANDER_INLINE PCF8574_PORT &PCF8574_PORT::operator=(uint8_t value)
        {
            // __DBG_printf("set PORT=%02x", value);
            _value = value;
            _updatePort();
            return *this;
        }

        IOEXPANDER_INLINE void PCF8574_PORT::_updatePort()
        {
            if (_parent->hasWire()) {
                // set all ports in INPUT mode to high and add all values for OUTPUT ports
                uint8_t value = (~_parent->DDR._value) | (_value & _parent->DDR._value);
                // __DBG_printf("write parent=%p PIN=%02x DDR=%02x PORT=%02x value=%02x", _parent, _parent->PIN._value, _parent->DDR._value, _parent->PORT._value, value);
                auto &wire = _parent->getWire();
                wire.beginTransmission(_parent->getAddress());
                wire.write(value);
                wire.endTransmission();
            }
        }

        // ------------------------------------------------------------------
        // PCF8574_DDR
        // ------------------------------------------------------------------

        IOEXPANDER_INLINE PCF8574_DDR &PCF8574_DDR::operator=(uint8_t value)
        {
            _value = value;
            _updatePort();
            return *this;
        }

        IOEXPANDER_INLINE void PCF8574_DDR::_updatePort()
        {
            _parent->PORT._updatePort();
        }

    }

    IOEXPANDER_INLINE PCF8574::PCF8574(uint8_t address, TwoWire *wire) :
        Base(address, wire),
        DDR(*this, 0x00),
        PIN(*this, 0x00),
        PORT(*this, 0xff)
    {
    }

    IOEXPANDER_INLINE void PCF8574::begin(uint8_t address, TwoWire *wire)
    {
        _wire = wire;
        begin(address);
    }

    IOEXPANDER_INLINE void PCF8574::begin(uint8_t address)
    {
        _address = address;
        DDR._value = 0xff; // set all ports to input
        PORT = 0xff; // set all ports to high
    }

    IOEXPANDER_INLINE void PCF8574::pinMode(uint8_t pin, uint8_t mode)
    {
        switch(mode) {
            case OUTPUT:
                DDR |= _BV(pin);
                break;
            case INPUT_PULLUP: // pulldown is not supported
                PORT |= _BV(pin);                       // High-level output current: -1mA
                // passthrough
            default:
            case INPUT:
                DDR &= ~_BV(pin);
                break;
        }

    }

    IOEXPANDER_INLINE void PCF8574::digitalWrite(uint8_t pin, uint8_t value)
    {
        if (value) {
            // PORT = PORT._value | _BV(pin);
            PORT |= _BV(pin);
        }
        else {
            // PORT = PORT._value & ~_BV(pin);
            PORT &= ~_BV(pin);
        }
    }

    IOEXPANDER_INLINE uint8_t PCF8574::digitalRead(uint8_t pin)
    {
        return (read8() & _BV(pin)) != 0;
    }

    IOEXPANDER_INLINE void PCF8574::write8(uint8_t value)
    {
        PORT = value;
    }

    IOEXPANDER_INLINE uint8_t PCF8574::read8()
    {
        return static_cast<uint8_t>(PIN);
    }

    // ------------------------------------------------------------------
    // TinyPwm
    // ------------------------------------------------------------------

    namespace TinyPwmNS {

        enum class Commands : uint8_t {
            ANALOG_WRITE = 0x10,            // 1 byte PWM value
            ANALOG_READ = 0x11,             // one byte for the input channel, 0 = pb4, 1 = pb3. returns int16_t
            SET_PWM_FREQUENCY = 0x51        // set PWM frequency, 2 byte uint16_t 488-65535 Hz
        };

    };

    IOEXPANDER_INLINE int TinyPwm::analogRead(uint8_t pin)
    {
        _wire->beginTransmission(_address);
        _wire->write(static_cast<uint8_t>(TinyPwmNS::Commands::ANALOG_READ));
        _wire->write(pin);
        if (_wire->endTransmission(true) == 0 && _wire->available()) {
            int16_t value;
            if (_wire->readBytes(reinterpret_cast<uint8_t *>(&value), sizeof(value)) == sizeof(value)) {
                return value;
            }
        }
        return 0;
    }

    IOEXPANDER_INLINE bool TinyPwm::analogWrite(uint8_t pin, uint8_t value)
    {
        _wire->beginTransmission(_address);
        _wire->write(static_cast<uint8_t>(TinyPwmNS::Commands::ANALOG_WRITE));
        _wire->write(value);
        // _wire->write(static_cast<uint8_t>(TinyPwmNS::Commands::ANALOG_WRITE_EX));
        // _wire->write(pin);
        // _wire->write(value);
        if (_wire->endTransmission(true) != 0) {
            __DBG_printf("endTransmission() failed");
            return false;
        }
        return true;
    }

}
