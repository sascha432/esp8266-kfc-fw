/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <wire.h>

#define IOEXPANDER_INCLUDE_HPP 1

namespace IOExpander {

    class PCF8574;

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

    };

    template<uint8_t _PinRangeStart, uint8_t _PinRangeEnd>
    class PCF8574_Range {
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
    };

    class PCF8574 {
    public:
        static constexpr uint8_t kDefaultAddress = 0x21;

        using DDRType = IOWrapper::PCF8574_DDR;
        using PINType = IOWrapper::PCF8574_PIN;
        using PORTType = IOWrapper::PCF8574_PORT;

    public:
    	PCF8574(uint8_t address = kDefaultAddress, TwoWire *wire = &Wire);

    	void begin(uint8_t address, TwoWire *wire);
        void begin(uint8_t address = kDefaultAddress);
        bool isConnected() const;
        uint8_t getAddress() const;
        TwoWire &getWire();
        bool hasWire() const;

	    void pinMode(uint8_t pin, uint8_t mode);
        void digitalWrite(uint8_t pin, uint8_t value);
        uint8_t digitalRead(uint8_t pin);
    	void write8(uint8_t value);
	    uint8_t read8();

    protected:
        TwoWire *_wire;
        uint8_t _address;

    public:
        DDRType DDR;
        PINType PIN;
        PORTType PORT;
    };

}

#if IOEXPANDER_INCLUDE_HPP
#include "IOExpander.hpp"
#endif
