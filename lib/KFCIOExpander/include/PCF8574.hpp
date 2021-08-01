/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"

#ifndef DEBUG_IOEXPANDER_PCF8574
#define DEBUG_IOEXPANDER_PCF8574 DEBUG_IOEXPANDER
#endif

#if DEBUG_IOEXPANDER_PCF8574
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

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
            auto &wire = _parent->getWire();
            if (wire.requestFrom(_parent->getAddress(), 0x01U) == 1) {
                _value = wire.read();
            }
            else {
                __DBG_printf("_readValue ERR available=%u", wire.available());
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
            // set all ports in INPUT mode to high and add all values for OUTPUT ports
            uint8_t value = (~_parent->DDR._value) | (_value & _parent->DDR._value);
            // __DBG_printf("write parent=%p PIN=%02x DDR=%02x PORT=%02x value=%02x", _parent, _parent->PIN._value, _parent->DDR._value, _parent->PORT._value, value);
            auto &wire = _parent->getWire();
            wire.beginTransmission(_parent->getAddress());
            wire.write(value);
            uint8_t error;
            if ((error = wire.endTransmission()) != 0) {
                __DBG_printf("_updatePort ERR value=%02x error=%u", value, error);
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
        PORT(*this, 0xff),
        _intPins(0)
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
            case INPUT_PULLUP:
                // High-level output current: -1mA
                PORT |= _BV(pin);
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
        return (readPort() & _BV(pin)) != 0;
    }

    IOEXPANDER_INLINE int PCF8574::analogRead(uint8_t pin)
    {
        return digitalRead(pin);
    }

    IOEXPANDER_INLINE void PCF8574::analogWrite(uint8_t pin, int val)
    {
        digitalWrite(pin, val);
    }

    IOEXPANDER_INLINE void PCF8574::writePort(uint8_t value)
    {
        PORT = value;
    }

    IOEXPANDER_INLINE uint8_t PCF8574::readPort()
    {
        return static_cast<uint8_t>(PIN);
    }


    IOEXPANDER_INLINE void PCF8574::enableInterrupts(uint8_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode)
    {
         // remove states for mask
        _intState &= ~pinMask;
        // add current states for masked pins
        _intState |= (readPort() & pinMask);
        // set interrupt mode
        for(uint8_t i = 0; i < 8; i++) {
            // _intMode &= ~modeMask;
        }
        // enable interrupts
        _intPins |= pinMask;
    }

    IOEXPANDER_INLINE void PCF8574::disableInterrupts(uint8_t pinMask)
    {
        _intPins &= ~pinMask;
    }

    IOEXPANDER_INLINE bool PCF8574::interruptsEnabled()
    {
        return (_intPins != 0);
    }

}
