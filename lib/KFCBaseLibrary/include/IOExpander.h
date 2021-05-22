/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <wire.h>

#define IOEXPANDER_INCLUDE_HPP 1

namespace IOExpander {

    class PCF8574;
    class TinyPwm;

    template<uint8_t _DefaultAddress>
    class Base {
    public:
        static constexpr uint8_t kDefaultAddress = _DefaultAddress;

    public:
        Base() : _address(0), _wire(nullptr)
        {
        }

        Base(uint8_t address, TwoWire *wire)  : _address(address), _wire(wire)
        {
        }

        void begin(uint8_t address, TwoWire *wire)
        {
            _address = address;
            _wire = wire;
        }

        void begin(uint8_t address)
        {
            _address = address;
        }

        void begin()
        {
        }

        bool isConnected() const
        {
            if (!hasWire()) {
                return false;
            }
            _wire->beginTransmission(_address);
            return (_wire->endTransmission() == 0);
        }

        uint8_t getAddress() const
        {
            return _address;
        }

        TwoWire &getWire()
        {
            return *_wire;
        }

        bool hasWire() const
        {
            return _wire != nullptr;
        }


    protected:
        uint8_t _address;
        TwoWire *_wire;
    };

    template<uint8_t _PinRangeStart, uint8_t _PinRangeEnd>
    class BaseRange {
    public:
        static bool inRange(uint8_t pin) {
            return pin >= _PinRangeStart && pin < _PinRangeEnd;
        }
        static uint8_t pin2DigitalPin(uint8_t pin) {
            return _PinRangeStart + pin;
        }
        static uint8_t digitalPin2Pin(uint8_t pin) {
            return pin - _PinRangeStart;
        }
        static uint8_t pin2AnalogPin(uint8_t pin) {
            return _PinRangeStart + pin;
        }
        static uint8_t analogPin2Pin(uint8_t pin) {
            return pin - _PinRangeStart;
        }
    };

    namespace IOWrapper {

        template<typename _Parent, typename _Ta, typename _Tb>
        class IOWrapper {
        public:
            IOWrapper(_Parent &parent, _Ta value) :
                _value(value),
                _parent(&parent)
            {
            }

            IOWrapper(_Parent *parent, _Ta value) :
                _value(value),
                _parent(parent)
            {
            }

            _Tb &operator|=(int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value | value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator|=(long unsigned int  value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value | value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator&=(int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value & value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator&=(long unsigned int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value & value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator=(int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator=(long unsigned int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value);
                return *reinterpret_cast<_Tb *>(this);
            }

            operator int() {
                return static_cast<int>(_value);
            }

            operator uint8_t() {
                return static_cast<uint8_t>(_value);
            }


        protected:
            friend PCF8574;
            friend TinyPwm;

            _Ta _value;
            _Parent *_parent;
        };

        class PCF8574_PIN;
        class PCF8574_PORT;
        class PCF8574_DDR;

        // read port data
        class PCF8574_PIN : public IOWrapper<PCF8574, uint8_t, PCF8574_PIN> {
        public:
            using IOWrapperType = IOWrapper<PCF8574, uint8_t, PCF8574_PIN>;
            using IOWrapperType::IOWrapper;
            using IOWrapperType::_parent;
            using IOWrapperType::_value;

            operator uint8_t();

        protected:
            uint8_t _readValue();
        };

        // write port data
        class PCF8574_PORT : public IOWrapper<PCF8574, uint8_t, PCF8574_PORT> {
        public:
            using IOWrapperType = IOWrapper<PCF8574, uint8_t, PCF8574_PORT>;
            using IOWrapperType::IOWrapperType;
            using IOWrapperType::_parent;
            using IOWrapperType::_value;
            using IOWrapperType::operator uint8_t;
            using IOWrapperType::operator=;
            using IOWrapperType::operator|=;
            using IOWrapperType::operator&=;

            PCF8574_PORT &operator=(uint8_t value);

        protected:
            friend PCF8574_DDR;

            void _updatePort();
        };

        // set port mode
        class PCF8574_DDR : public IOWrapper<PCF8574, uint8_t, PCF8574_DDR> {
        public:
            using IOWrapperType = IOWrapper<PCF8574, uint8_t, PCF8574_DDR>;
            using IOWrapperType::IOWrapperType;
            using IOWrapperType::_parent;
            using IOWrapperType::_value;
            using IOWrapperType::operator uint8_t;
            using IOWrapperType::operator=;
            using IOWrapperType::operator|=;
            using IOWrapperType::operator&=;

            PCF8574_DDR &operator=(uint8_t value);

        protected:
            void _updatePort();
        };

    }

    template <uint8_t _PinRangeStart, uint8_t _PinRangeEnd>
    using PCF8574_Range =  BaseRange<_PinRangeStart, _PinRangeEnd>;

    class PCF8574 : public Base<0x21> {
    public:
        using DDRType = IOWrapper::PCF8574_DDR;
        using PINType = IOWrapper::PCF8574_PIN;
        using PORTType = IOWrapper::PCF8574_PORT;

    public:
        using Base = Base<0x21>;
        using Base::Base;
        using Base::begin;

    public:
    	PCF8574(uint8_t address = kDefaultAddress, TwoWire *wire = &Wire);

    	 void begin(uint8_t address, TwoWire *wire);
        void begin(uint8_t address);

	    void pinMode(uint8_t pin, uint8_t mode);
        void digitalWrite(uint8_t pin, uint8_t value);
        uint8_t digitalRead(uint8_t pin);
    	void write8(uint8_t value);
	    uint8_t read8();

    public:
        DDRType DDR;
        PINType PIN;
        PORTType PORT;
    };

    // ATTiny853 IO expander with 1 PWM output and 2 ADC inputs

    template <uint8_t _PinRangeStart, uint8_t _PinRangeEnd>
    using TinyPwm_Range =  BaseRange<_PinRangeStart, _PinRangeEnd>;

    class TinyPwm : public Base<0x60> {
    public:
        using Base = Base<0x60>;
        using Base::Base;
        using Base::begin;

        static constexpr uint8_t PB1 = 1;

        // TinyPwm(uint8_t address = kDefaultAddress, TwoWire *wire = &Wire);

        int analogRead(uint8_t pinNo);
        bool analogWrite(uint8_t pin, uint8_t value);

        void pinMode(uint8_t pin, uint8_t mode) {}
    };

}

#if IOEXPANDER_INCLUDE_HPP
#include "IOExpander.hpp"
#endif
