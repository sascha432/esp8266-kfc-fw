/**
  Author: sascha_lammers@gmx.de
*/

#include "IOExpander.h"

#ifndef DEBUG_IOEXPANDER_MCP23017
#define DEBUG_IOEXPANDER_MCP23017 1
#endif

#if DEBUG_IOEXPANDER_MCP23017
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

namespace IOExpander {

    #undef IOEXPANDER_INLINE
    #define IOEXPANDER_INLINE

    #if DEBUG_IOEXPANDER_MCP23017

        IOEXPANDER_INLINE const __FlashStringHelper *__regAddrName(uint8_t addr) {
            switch(addr) {
                case MCP23017::IODIR:
                    return F("IODIRA");
                case MCP23017::IODIR + MCP23017::PORT_BANK_INCREMENT:
                    return F("IODIRB");
                case MCP23017::IPOL:
                    return F("IPOLA");
                case MCP23017::IPOL + MCP23017::PORT_BANK_INCREMENT:
                    return F("IPOLB");
                case MCP23017::GPINTEN:
                    return F("GPINTENA");
                case MCP23017::GPINTEN + MCP23017::PORT_BANK_INCREMENT:
                    return F("GPINTENB");
                case MCP23017::DEFVAL:
                    return F("DEFVALA");
                case MCP23017::DEFVAL + MCP23017::PORT_BANK_INCREMENT:
                    return F("DEFVALB");
                case MCP23017::INTCON:
                    return F("INTCONA");
                case MCP23017::INTCON + MCP23017::PORT_BANK_INCREMENT:
                    return F("INTCONB");
                case MCP23017::IOCON:
                    return F("IOCONA");
                case MCP23017::IOCON + MCP23017::PORT_BANK_INCREMENT:
                    return F("IOCONB");
                case MCP23017::GPPU:
                    return F("GPPUA");
                case MCP23017::GPPU + MCP23017::PORT_BANK_INCREMENT:
                    return F("GPPUB");
                case MCP23017::INTF:
                    return F("INTFA");
                case MCP23017::INTF + MCP23017::PORT_BANK_INCREMENT:
                    return F("INTFB");
                case MCP23017::INTCAP:
                    return F("INTCAPA");
                case MCP23017::INTCAP + MCP23017::PORT_BANK_INCREMENT:
                    return F("INTCAPB");
                case MCP23017::GPIO:
                    return F("GPIOA");
                case MCP23017::GPIO + MCP23017::PORT_BANK_INCREMENT:
                    return F("GPIOB");
            }
            return F("N/A");
        }

    #endif

    IOEXPANDER_INLINE MCP23017::MCP23017(uint8_t address, TwoWire *wire) :
        Base(address, wire),
        _interruptsPending(0)
    {
    }

    IOEXPANDER_INLINE void MCP23017::begin(uint8_t address, TwoWire *wire)
    {
        _wire = wire;
        begin(address);
    }

    IOEXPANDER_INLINE void MCP23017::begin(uint8_t address)
    {
        // default values for all register except _IODIR are 0
        _address = address;
        // set all pins to input
        _IODIR = 0xffff;
        _write16(IODIR, _IODIR);
        // disable pullups
        _GPPU = 0;
        _write16(GPPU, _GPPU);
        // disable interrupts
        _INTCON = 0;
        _DEFVAL = 0;
        _GPINTEN = 0;
        _write16(GPINTEN, _GPINTEN);
        // set gpio values to 0
        _GPIO = 0;
    }

    IOEXPANDER_INLINE int MCP23017::analogRead(uint8_t pinNo)
    {
        return digitalRead(pinNo);
    }

    IOEXPANDER_INLINE bool MCP23017::analogWrite(uint8_t pin, uint8_t value)
    {
        return false;
    }

    IOEXPANDER_INLINE void MCP23017::digitalWrite(uint8_t pin, uint8_t value)
    {
        uint8_t mask;
        auto port = _pin2PortAndMask(pin, mask);
        _setBits(_GPIO, port, mask, value);
        _write8(GPIO, _GPIO, port);
    }

    IOEXPANDER_INLINE uint8_t MCP23017::digitalRead(uint8_t pin)
    {
        uint8_t mask;
        Port port = _pin2PortAndMask(pin, mask);
        _read8(GPIO, _GPIO, port);
        return _getBits(_GPIO, port, mask) ? 1 : 0;
    }

    IOEXPANDER_INLINE uint8_t MCP23017::readPortA()
    {
        _read8(GPIO, _GPIO, Port::A);
        return _GPIO.A;
    }

    IOEXPANDER_INLINE uint8_t MCP23017::readPortB()
    {
        _read8(GPIO, _GPIO, Port::B);
        return _GPIO.B;
    }

    IOEXPANDER_INLINE uint16_t MCP23017::readPortAB()
    {
        _read16(GPIO, _GPIO);
        return _GPIO._value;
    }

    IOEXPANDER_INLINE void MCP23017::writePortA(uint8_t value)
    {
        _GPIO.A = value;
        _write8(GPIO, _GPIO, Port::A);
    }

    IOEXPANDER_INLINE void MCP23017::writePortB(uint8_t value)
    {
        _GPIO.B = value;
        _write8(GPIO, _GPIO, Port::B);
    }

    IOEXPANDER_INLINE void MCP23017::writePortAB(uint16_t value)
    {
        _GPIO._value = value;
        _write16(GPIO, _GPIO);
    }

    IOEXPANDER_INLINE void MCP23017::pinMode(uint8_t pin, uint8_t mode)
    {
        __LDBG_printf("pinMode %u=%u", pin, mode);
        uint8_t mask;
        auto port = _pin2PortAndMask(pin, mask);
        _setBits(_IODIR, port, mask, mode != OUTPUT);
        _setBits(_GPPU, port, mask, mode == INPUT_PULLUP);
        _write8(IODIR, _IODIR, port);
        _write8(GPPU, _GPPU, port);
    }

    IOEXPANDER_INLINE void MCP23017::enableInterrupts(uint16_t pinMask, const InterruptCallback &callback, uint8_t mode)
    {
        ets_intr_lock();
        _interruptsPending = 0;
        _callback = callback;
        ets_intr_unlock();
        _IOCON.A = IOCON_MIRROR|IOCON_INTPOL;
        _write8(IOCON, _IOCON, Port::A);

        if (mode == CHANGE) {
            _INTCON._value &= ~pinMask; // interrupt on change
        }
        else {
            // setup DEFVAL
            if (mode == FALLING) {
                _DEFVAL._value |= pinMask;
                _write16(DEFVAL, _DEFVAL);
            }
            else if (mode == RISING) {
                _DEFVAL._value &= ~pinMask;
                _write16(DEFVAL, _DEFVAL);
            }
            _INTCON._value |= pinMask; // compare against DEFVAL
        }
        _write16(INTCON, _INTCON);

        _GPINTEN._value |= pinMask;
        _write16(GPINTEN, _GPINTEN);
    }

    IOEXPANDER_INLINE void MCP23017::disableInterrupts(uint16_t pinMask)
    {
        _GPINTEN._value &= ~pinMask;
        _write16(GPINTEN, _GPINTEN);

        if (_GPINTEN._value == 0) {

            _INTCON = 0;
            _DEFVAL = 0;
            _read16(INTCAP, _INTCAP); // clear pending interrupts
            _INTCAP._value = 0;

            ets_intr_lock();
            _interruptsPending = 0;
            _callback = nullptr;
            ets_intr_unlock();
        }
    }

    IOEXPANDER_INLINE bool MCP23017::interruptsEnabled()
    {
        return _GPINTEN._value != 0;
    }

    IOEXPANDER_INLINE void MCP23017::invokeCallback()
    {
        _callback(readPortAB()); // TODO read captured pin state
    }

    IOEXPANDER_INLINE void MCP23017::interruptHandler()
    {
        uint32_t start = micros();
        ets_intr_lock();
        while (_interruptsPending) {
            _interruptsPending = 0;
            ets_intr_unlock();

            invokeCallback();

            // check again if any new interupts occured while processing
            ets_intr_lock();
            if (_interruptsPending) {
                if (micros() - start > 2000) {
                    // abort after 2ms and reschedule
                    schedule_function([this]() {
                        interruptHandler();
                    });
                    break;
                }
            }
        }
        ets_intr_unlock();
    }

    IOEXPANDER_INLINE uint8_t MCP23017::_portAddress(uint8_t regAddr, Port port) const
    {
        if (port == Port::A) {
            return regAddr;
        }
        else {
            return regAddr + PORT_BANK_INCREMENT;
        }
    }

    IOEXPANDER_INLINE uint8_t MCP23017::_getBits(Register16 regValue, Port port, uint8_t mask)
    {
        return regValue[port] & mask;
    }

    IOEXPANDER_INLINE void MCP23017::_setBits(Register16 &regValue, Port port, uint8_t mask, bool value)
    {
        auto &target = regValue[port];
        if (value) {
            target |= mask;
        }
        else {
            target &= ~mask;
        }
    }

    IOEXPANDER_INLINE MCP23017::Port MCP23017::_pin2Port(uint8_t pin) const
    {
        return (pin < 8) ? Port::A : Port::B;
    }

    IOEXPANDER_INLINE MCP23017::Port MCP23017::_pin2PortAndMask(uint8_t pin, uint8_t &mask) const
    {
        if (pin < 8) {
            mask = _BV(pin);
            return Port::A;
        }
        else {
            mask = _BV(pin - 8);
            return Port::B;
        }
    }

    IOEXPANDER_INLINE void MCP23017::_write16(uint8_t regAddr, Register16 &regValue)
    {
        uint8_t error;
        regAddr = _portAddress<Port::A>(regAddr);
        _wire->beginTransmission(_address);
        _wire->write(regAddr);
        _wire->write(reinterpret_cast<uint8_t *>(&regValue._value), 2);
        if ((error = _wire->endTransmission(true)) != 0) {
            __DBG_printf("_write16 ERR regAddr=%s A=%02x B=%02x error=%u", __regAddrName(regAddr), regValue.A, regValue.B, error);
        }
        // __LDBG_printf("_write16 %s A=%02x B=%02x", __regAddrName(regAddr), regValue.A, regValue.B);
    }

    IOEXPANDER_INLINE void MCP23017::_read16(uint8_t regAddr, Register16 &regValue)
    {
        uint8_t error;
        regAddr = _portAddress<Port::A>(regAddr);
        _wire->beginTransmission(_address);
        _wire->write(regAddr);
        if ((error = _wire->endTransmission(false)) != 0) {
            __DBG_printf("_read16 ERR reg_addr=%s error=%u", __regAddrName(regAddr), error);
            return;
        }
        if (_wire->requestFrom(_address, 2U, true) != 2) {
            __DBG_printf("_read16 ERR request=2 reg_addr=%s available=%u", __regAddrName(regAddr), _wire->available());
            return;
        }
        _wire->readBytes(reinterpret_cast<uint8_t *>(&regValue._value), 2);
        // __LDBG_printf("_read16 reg_addr=%s A=%02x B=%02x", __regAddrName(regAddr), regValue.A, regValue.B);
    }

    IOEXPANDER_INLINE void MCP23017::_write8(uint8_t regAddr, Register16 regValue, Port port)
    {
        uint8_t error;
        auto source = regValue[port];
        regAddr = _portAddress(regAddr, port);
        _wire->beginTransmission(_address);
        _wire->write(regAddr);
        _wire->write(source);
        if ((error = _wire->endTransmission(true)) != 0) {
            __DBG_printf("_write8 ERR reg_addr=%s value=%02x error=%u", __regAddrName(regAddr), source, error);
        }
        // __LDBG_printf("_write8 reg_addr=%s %c=%02x", __regAddrName(regAddr), port == Port::A ? 'A' : 'B', source);
    }

    IOEXPANDER_INLINE void MCP23017::_read8(uint8_t regAddr, Register16 &regValue, Port port)
    {
        uint8_t error;
        auto &target = regValue[port];
        _wire->beginTransmission(_address);
        regAddr = _portAddress(regAddr, port);
        _wire->write(regAddr);
        if ((error = _wire->endTransmission(false)) != 0) {
            __DBG_printf("_read8 ERR reg_addr=%s error=%u", __regAddrName(regAddr), error);
            return;
        }
        if (_wire->requestFrom(_address, 1U, true) != 1) {
            __DBG_printf("_read8 ERR request=1 reg_addr=%s available=%u", __regAddrName(regAddr), _wire->available());
            return;
        }
        target = _wire->read();
        // __LDBG_printf("_read8 reg_addr=%s %c=%02x", __regAddrName(regAddr), port == Port::A ? 'A' : 'B', target);
    }

}
