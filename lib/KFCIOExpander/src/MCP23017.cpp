/**
  Author: sascha_lammers@gmx.de
*/

#include "IOExpander.h"

namespace IOExpander {

    #undef IOEXPANDER_INLINE
    #define IOEXPANDER_INLINE

    IOEXPANDER_INLINE MCP23017::MCP23017(uint8_t address, TwoWire *wire) : Base(address, wire), _interruptsPending(0)
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
        __DBG_printf("IODIR %02x", _IODIR._value);
        _write16(IODIR, _IODIR);
        // disable pullups
        _GPPU = 0;
        __DBG_printf("GPPU %02x", _GPPU._value);
        _write16(GPPU, _GPPU);
        // disable interrupts
        _GPINTEN = 0;
        __DBG_printf("_GPINTEN %02x", _GPINTEN._value);
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
        __DBG_printf("digitalWrite %u=%u", pin, value);
        _setBits(_GPIO, _pin2Port(pin), _BV(pin), value);
        _write8(GPIO, _GPIO, _pin2Port(pin));
    }

    IOEXPANDER_INLINE uint8_t MCP23017::digitalRead(uint8_t pin)
    {
        _read8(GPIO, _GPIO, _pin2Port(pin));
        return _getBits(_GPIO, _pin2Port(pin), _BV(pin)) ? 1 : 0;
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
        __DBG_printf("pinMode %u=%u", pin, mode);
        _setBits(_IODIR, _pin2Port(pin), _BV(pin), mode == OUTPUT);
        _setBits(_GPPU, _pin2Port(pin), _BV(pin), mode == INPUT_PULLUP);
        __DBG_printf("IODIR %02x", _IODIR._value);
        _write8(IODIR, _IODIR, _pin2Port(pin));
        __DBG_printf("GPPU %02x", _GPPU._value);
        _write8(GPPU, _GPPU, _pin2Port(pin));
    }

    IOEXPANDER_INLINE void MCP23017::enableInterrupts(uint16_t pinMask, const InterruptCallback &callback, uint8_t mode)
    {
        ets_intr_lock();
        _interruptsPending = 0;
        _callback = callback;
        ets_intr_unlock();
        _IOCON.A = IOCON_MIRROR|IOCON_INTPOL;
        _write8(IOCON, _IOCON, Port::A);

        if (mode == FALLING) {
            _DEFVAL._value |= pinMask;
            _write16(DEFVAL, _DEFVAL);
        }
        else if (mode == RISING) {
            _DEFVAL._value &= ~pinMask;
            _write16(DEFVAL, _DEFVAL);
        }

        if (mode == CHANGE) {
            _INTCON._value &= ~pinMask; // compare against DEFVAL
        }
        else {
            _INTCON._value |= pinMask;
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

    IOEXPANDER_INLINE void MCP23017::interruptHandler()
    {
        uint32_t start = micros();
        ets_intr_lock();
        while (_interruptsPending) {
            _interruptsPending = 0;
            ets_intr_unlock();

            _callback(0);

            // check again if any new interupts occured while reading
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

    IOEXPANDER_INLINE uint8_t MCP23017::_portAddress(uint8_t regAddr, Port port)
    {
        if (port == Port::A) {
            return regAddr;
        }
        if (port == Port::B) {
            return regAddr + PORT_BANK_INCREMENT;
        }
        return 0xff;
    }

    IOEXPANDER_INLINE uint8_t MCP23017::_getBits(Register16 regValue, Port port, uint8_t mask)
    {
        auto source = regValue.BA;
        if (port == Port::A) {
            source++;
        }
        return *source & mask;
    }

    IOEXPANDER_INLINE void MCP23017::_setBits(Register16 &regValue, Port port, uint8_t mask, bool value)
    {
        auto target = regValue.BA;
        if (port == Port::A) {
            target++;
        }
        if (value) {
            *target |= mask;
        }
        else {
            *target &= ~mask;
        }
    }

    IOEXPANDER_INLINE MCP23017::Port MCP23017::_pin2Port(uint8_t pin)
    {
        return (pin < 8) ? Port::A : Port::B;
    }

    IOEXPANDER_INLINE void MCP23017::_write16(uint8_t regAddr, Register16 &regValue)
    {
        if constexpr (PORT_BANK_INCREMENT == 1) {
            //TODO change to bulk write
            _write8(regAddr, regValue, Port::A);
            _write8(regAddr, regValue, Port::B);
        }
        else {
            _write8(regAddr, regValue, Port::A);
            _write8(regAddr, regValue, Port::B);
        }
    }

    IOEXPANDER_INLINE void MCP23017::_read16(uint8_t regAddr, Register16 &regValue)
    {
        if constexpr (PORT_BANK_INCREMENT == 1) {
            //TODO change to bulk write
            _read8(regAddr, regValue, Port::A);
            _read8(regAddr, regValue, Port::B);
        }
        else {
            _read8(regAddr, regValue, Port::A);
            _read8(regAddr, regValue, Port::B);
        }
    }

    IOEXPANDER_INLINE void MCP23017::_write8(uint8_t regAddr, Register16 regValue, Port port)
    {
        auto source = regValue.BA;
        if (port == Port::A) {
            source++;
        }
        __DBG_printf("_write8 %02x: %02x=%02x OP=%02x val16=%04x", _address, _portAddress(regAddr, port), *source, _OPW(), regValue._value);

        uint8_t error;
        _wire->beginTransmission(_address);
        _wire->write(_OPW());
        _wire->write(_portAddress(regAddr, port));
        // if ((error = _wire->endTransmission(false)) != 0) {
        //     __DBG_printf("WRITE ERROR reg_addr=%02x(%u) write cmd error=%u available=%u", regAddr, port, error, _wire->available());
        //     return;
        // }
        // _wire->beginTransmission(_address);
        _wire->write(*source);
        if ((error = _wire->endTransmission(true)) != 0) {
            __DBG_printf("_write8 regaddr=%02x(%u) cmd=%02x", _portAddress(regAddr, port), port, _OPW());
        }
    }

    IOEXPANDER_INLINE void MCP23017::_read8(uint8_t regAddr, Register16 &regValue, Port port)
    {
        auto target = regValue.BA;
        if (port == Port::A) {
            target++;
        }

        uint32_t start = micros();
        uint32_t dur;

        uint8_t error;
        _wire->beginTransmission(_address);
        _wire->write(_OPW());
        _wire->write(_portAddress(regAddr, port));
        if ((error = _wire->endTransmission(false)) != 0) {
            __DBG_printf("_read8 regaddr=%02x(%u) cmd=%02x", _portAddress(regAddr, port), port, _OPW());
            return;
        }
        _wire->write(_OPR());
        if (_wire->endTransmission(false) != 0) {
            __DBG_printf("_read8 regaddr=%02x(%u) cmd=%02x", _portAddress(regAddr, port), port, _OPR());
            return;
        }
        if (_wire->requestFrom(_address, 1U, true) != 1) {
            __DBG_printf("_read8 regaddr=%02x(%u) available=%u", _portAddress(regAddr, port), port, _wire->available());
            return;
        }

        *target = _wire->read();
        dur = micros() - start;
        __DBG_printf("read8 regaddr=%02x(%u) time=%u value=%02x val16=%04x", _portAddress(regAddr, port), port, dur, *target, regValue._value);
    }

    IOEXPANDER_INLINE uint8_t MCP23017::_OPR() {
        return (_address << 1) | 1;
    }

    IOEXPANDER_INLINE uint8_t MCP23017::_OPW() {
        return (_address << 1);
    }

}
