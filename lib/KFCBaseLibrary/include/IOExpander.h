/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <wire.h>

namespace IOExpander {

    class PCF8574;

    // using PCF8574

    namespace IOWrapper {

        template<typename _Parent, typename _Ta>
        class IOWrapper {
        public:
            IOWrapper(_Parent &parent, _Ta value) :
                _value(value),
                _parent(&parent)
            {}

            IOWrapper(_Parent *parent, _Ta value) :
                _value(value),
                _parent(parent)
            {}

        protected:
            _Ta &_value;
            _Parent *_parent;
        };


        // read port data
        class PCF8574_PIN : public IOWrapper<PCF8574, uint8_t> {
        public:
            using IOWrapperType = IOWrapper<PCF8574, uint8_t>;
            using IOWrapperType::IOWrapper;
            using IOWrapperType::_parent;
            using IOWrapperType::_value;

            operator int() {
                return _readValue();
            }

        protected:
            uint8_t _readValue();
        };

        // write port data
        class PCF8574_PORT : public IOWrapper<PCF8574, uint8_t> {
        public:
            using IOWrapperType = IOWrapper<PCF8574, uint8_t>;
            using IOWrapperType::IOWrapperType;
            using IOWrapperType::_parent;
            using IOWrapperType::_value;

            PCF8574_PORT &operator=(int value) {
                *this = static_cast<uint8_t>(value);
                return *this;
            }
            PCF8574_PORT &operator=(long unsigned int value) {
                *this = static_cast<uint8_t>(value);
                return *this;
            }
            PCF8574_PORT &operator=(uint8_t value);

        protected:
            void _updatePort();
        };

        // set port mode
        class PCF8574_DDR : public PCF8574_PORT {
        public:
            using PCF8574_PORT::PCF8574_PORT;
            using PCF8574_PORT::_parent;
            using PCF8574_PORT::_value;
            using PCF8574_PORT::_updatePort;

            operator int() const {
                return static_cast<int>(_value);
            }
            PCF8574_DDR &operator=(int value) {
                *this = static_cast<uint8_t>(value);
                return *this;
            }
            PCF8574_DDR &operator=(long unsigned int value) {
                *this = static_cast<uint8_t>(value);
                return *this;
            }
            PCF8574_DDR &operator=(uint8_t value);
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
    	PCF8574(uint8_t address = kDefaultAddress, TwoWire &wire = Wire) :
            _wire(wire),
            _address(address),
            DDR(*this, 0x00),
            PIN(*this, 0x00),
            PORT(*this, 0xff)
        {}

    	void begin(uint8_t address = kDefaultAddress) {
            _address = address;
            DDR = 0xff; // set all ports to input
            PORT = 0xff; // set all ports to high
        }

        bool isConnected() const {
            _wire.beginTransmission(_address);
            return (_wire.endTransmission() == 0);
        }

        uint8_t getAddress() const {
            return _address;
        }

        TwoWire &getWire() {
            return _wire;
        }

	    void pinMode(uint8_t pin, uint8_t mode) {
            switch(mode) {
                case INPUT:
                case INPUT_PULLUP: // pulldown is not supported
                    DDR = DDR & ~(_BV(pin));
                    PORT = PIN & _BV(pin);
                    break;
                case OUTPUT:
                    DDR = DDR | _BV(pin);
                    PORT = PIN | _BV(pin);
                    break;
            }

        }

    	void digitalWrite(uint8_t pin, uint8_t value) {
            if (value) {
                PORT = PIN | _BV(pin);
            }
            else {
                PORT = PIN & ~_BV(pin);
            }
        }

        uint8_t digitalRead(uint8_t pin) {
            return (static_cast<int>(PIN) & _BV(pin)) != 0;
        }

    	void write8(uint8_t value) {
            PORT = value;
        }

	    uint8_t read8() {
            return static_cast<int>(PIN);
        }

    protected:
        TwoWire &_wire;
        uint8_t _address;

    public:
        DDRType DDR;
        PINType PIN;
        PORTType PORT;
    };

    using namespace IOWrapper;

        inline void PCF8574_PORT::_updatePort() {
            // set all ports in INPUT mode to high and add all values for OUTPUT ports
            uint8_t value = (~_parent->DDR) | (_value & _parent->DDR);

            _parent->getWire().beginTransmission(_parent->getAddress());
            _parent->getWire().write(value);
            _parent->getWire().endTransmission();
        }

        inline uint8_t PCF8574_PIN::_readValue()
        {
            int data;
            auto &wire =  _parent->getWire();
            if (wire.requestFrom(_parent->getAddress(), (uint8_t) 0x01) == 1 && wire.available() && (data = wire.read()) != -1) {
                _value = data;
            }
            return _value;
        }

        inline PCF8574_PORT &PCF8574_PORT::operator=(uint8_t value) {
            _value = value;
            _updatePort();
            return *this;
        }

        inline PCF8574_DDR &PCF8574_DDR::operator=(uint8_t value) {
            _value = value;
            _updatePort();
            return *this;
        }


}
