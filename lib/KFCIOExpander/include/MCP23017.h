/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"

namespace IOExpander {

    class MCP23017 : public Base<DeviceTypeMCP23017, MCP23017> {
    public:
        // bank1, the registers associated with each port are separated into different bank
        // static constexpr uint8_t PORT_BANK_MULTIPLIER = 1;
        // static constexpr uint8_t PORT_BANK_INCREMENT = 0x10;

        // bank0 default, addresses are sequential
        static constexpr uint8_t PORT_BANK_MULTIPLIER = 2;
        static constexpr uint8_t PORT_BANK_INCREMENT = 1;

        static constexpr uint8_t IODIR = 0x00 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t IPOL = 0x01 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t GPINTEN = 0x02 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t DEFVAL = 0x03 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t INTCON = 0x04 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t IOCON = 0x05 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t GPPU = 0x06 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t INTF = 0x07 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t INTCAP = 0x08 * PORT_BANK_MULTIPLIER;
        static constexpr uint8_t GPIO = 0x09 * PORT_BANK_MULTIPLIER;

        // INTPOL: This bit sets the polarity of the INT output pin
        // 1 = Active-high
        // 0 = Active-low
        static constexpr uint8_t IOCON_INTPOL = _BV(1);
        // ODR: Configures the INT pin as an open-drain output
        // 1 = Open-drain output (overrides the INTPOL bit.)
        // 0 = Active driver output (INTPOL bit sets the polarity.)
        static constexpr uint8_t IOCON_ODR = _BV(2);
        // DISSLW: Slew Rate control bit for SDA output
        // 1 = Slew rate disabled
        // 0 = Slew rate enabled
        static constexpr uint8_t IOCON_DISSLW = _BV(4);
        // SEQOP: Sequential Operation mode bit
        // 1 = Sequential operation disabled, address pointer does not increment.
        // 0 = Sequential operation enabled, address pointer increments.
        static constexpr uint8_t IOCON_SEQOP = _BV(5);
        // MIRROR: INT Pins Mirror bit
        // 1 = The INT pins are internally connected
        // 0 = The INT pins are not connected. INTA is associated with PORTA and INTB is associated with PORTB
        static constexpr uint8_t IOCON_MIRROR = _BV(6);
        // BANK: Controls how the registers are addressed
        // 1 = The registers associated with each port are separated into different banks.
        // 0 = The registers are in the same bank (addresses are sequential)
        static constexpr uint8_t IOCON_BANK = _BV(7);

        enum class Port : uint8_t {
            A,
            B,
        };

        struct PortAndMask {
            Port port;
            uint8_t mask;
            PortAndMask(Port _port, uint8_t _mask) : port(_port), mask(_mask) {}
        };

        struct Register16 {
            union {
                uint16_t _value;
                struct __attribute__((__packed__)) {
                    uint8_t A;
                    uint8_t B;
                };
            };

            // 16bit operations

            Register16() : _value(0) {}
            Register16(uint16_t value) : _value(value) {}

            inline  __attribute__((__always_inline__))
            operator uint16_t() const {
                return _value;
            }

            inline  __attribute__((__always_inline__))
            Register16 &operator =(uint16_t value) {
                _value = value;
                return *this;
            }

            inline  __attribute__((__always_inline__))
            Register16 &operator |=(uint16_t value) {
                _value |= value;
                return *this;
            }

            inline  __attribute__((__always_inline__))
            Register16 &operator &=(uint16_t value) {
                _value &= value;
                return *this;
            }

            // 8bit operations

            inline  __attribute__((__always_inline__))
            uint8_t &operator [](Port port) {
                return port == Port::A ? A : B;
            }

            inline  __attribute__((__always_inline__))
            uint8_t operator [](Port port) const {
                return port == Port::A ? A : B;
            }

            inline  __attribute__((__always_inline__))
            uint8_t get(Port port, uint8_t mask) {
                return (*this)[port] & mask;
            }

            inline  __attribute__((__always_inline__))
            void set(Port port, uint8_t mask) {
                (*this)[port] |= mask;
            }

            inline  __attribute__((__always_inline__))
            void unset(Port port, uint8_t mask) {
                (*this)[port] &= ~mask;
            }

            inline  __attribute__((__always_inline__))
            void set(Port port, uint8_t mask, bool value) {
                if (value) {
                    set(port, mask);
                }
                else {
                    unset(port, mask);
                }
            }

            template<Port _Port>
            inline  __attribute__((__always_inline__))
            uint8_t get(uint8_t mask) {
                return (_Port == Port::A ? A : B) & mask;
            }

            template<Port _Port>
            inline  __attribute__((__always_inline__))
            void reset(uint8_t value = 0) {
                (_Port == Port::A ? A : B) = value;
            }

            template<Port _Port>
            inline  __attribute__((__always_inline__))
            void set(uint8_t mask) {
                (_Port == Port::A ? A : B) |= mask;
            }

            template<Port _Port>
            inline  __attribute__((__always_inline__))
            void unset(uint8_t mask) {
                (_Port == Port::A ? A : B) &= ~mask;
            }
        };

    public:
        using Base = Base<DeviceTypeMCP23017, MCP23017>;
        using Base::Base;
        using DeviceType = Base::DeviceType;
        using DeviceClassType = Base::DeviceClassType;
        using Base::begin;
        using Base::kDefaultAddress;
        using Base::kDeviceType;

        MCP23017(uint8_t address = kDefaultAddress, TwoWire *wire = &Wire);

    	void begin(uint8_t address, TwoWire *wire);
        void begin(uint8_t address);

        int analogRead(uint8_t pinNo);
        bool analogWrite(uint8_t pin, uint8_t value);

        void digitalWrite(uint8_t pin, uint8_t value);
        uint8_t digitalRead(uint8_t pin);

        uint8_t readPortA();
        uint8_t readPortB();
        uint16_t readPortAB();

        inline  __attribute__((__always_inline__))
        uint16_t readPort() {
            return readPortAB();
        }

        void writePortA(uint8_t value);
        void writePortB(uint8_t value);
        void writePortAB(uint16_t value);

        inline  __attribute__((__always_inline__))
        void writePort(uint16_t value) {
            writePortAB(value);
        }

        void pinMode(uint8_t pin, uint8_t mode);

        void analogReference(uint8_t mode) {}
        void analogWrite(uint8_t pin, int val) {}
        void analogWriteFreq(uint32_t freq) {}

        // currently only mirrored interrupts for port A&B are supported
        void enableInterrupts(uint16_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode);
        void disableInterrupts(uint16_t pinMask);
        bool interruptsEnabled();
        void invokeCallback();

        void interruptHandler();
        bool setInterruptFlag();

    protected:
        // get port for pin #
        Port _pin2Port(uint8_t pin) const;
        // get port for pin # and mask
        PortAndMask _pin2PortAndMask(uint8_t pin) const;
        // get register adddress for port A or B
        uint8_t _portAddress(uint8_t regAddr, Port port) const;
        // get register adddress for port A or B
        template<Port _Port>
        constexpr uint8_t _portAddress(uint8_t regAddr) const {
            return (_Port == Port::A) ? regAddr : regAddr + PORT_BANK_INCREMENT;
        }
        // write 16 bit register
        void _write16(uint8_t regAddr, Register16 &regValue);
        // read 16 bit register
        void _read16(uint8_t regAddr, Register16 &regValue);
        // write 8bit part of a 16 bit register
        void _write8(uint8_t regAddr, Register16 regValue, Port port);
        // read 8bit part of a 16 bit register
        void _read8(uint8_t regAddr, Register16 &regValue, Port port);

    public:
        uint32_t _interruptsPending;
        InterruptCallback _callback;

    protected:
        Register16 _IODIR;
        Register16 _GPPU;
        Register16 _GPIO;
        Register16 _IOCON;
        Register16 _GPINTEN;
        Register16 _INTCON;
        Register16 _DEFVAL;
        Register16 _INTCAP;
        // Register16 _INTF;
        // Register16 _IPOL;
    };

    inline  __attribute__((__always_inline__))
    bool MCP23017::setInterruptFlag() {
        _interruptsPending++;
        return _interruptsPending - 1;
    }

}

