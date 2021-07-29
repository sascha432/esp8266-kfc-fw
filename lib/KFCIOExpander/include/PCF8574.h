/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"

namespace IOExpander {

    namespace IOWrapper {

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

    class PCF8574 : public Base<DeviceTypePCF8574, PCF8574> {
    public:
        using DDRType = IOWrapper::PCF8574_DDR;
        using PINType = IOWrapper::PCF8574_PIN;
        using PORTType = IOWrapper::PCF8574_PORT;

    public:
        using Base = Base<DeviceTypePCF8574, PCF8574>;
        using Base::Base;
        using DeviceType = Base::DeviceType;
        using DeviceClassType = Base::DeviceClassType;
        using Base::begin;
        using Base::kDefaultAddress;
        using Base::kDeviceType;

    public:
    	PCF8574(uint8_t address = kDefaultAddress, TwoWire *wire = &Wire);

    	void begin(uint8_t address, TwoWire *wire);
        void begin(uint8_t address);

	    void pinMode(uint8_t pin, uint8_t mode);
        void digitalWrite(uint8_t pin, uint8_t value);
        uint8_t digitalRead(uint8_t pin);
    	void write8(uint8_t value);
	    uint8_t read8();

        int analogRead(uint8_t pin) {
            return digitalRead(pin);
        }

        void analogWrite(uint8_t pin, int val) {
            digitalWrite(pin, val);
        }

        void analogReference(uint8_t mode) {}
        void analogWriteFreq(uint32_t freq) {}

        // interrupts are not supported
        inline  __attribute__((__always_inline__))
        void enableInterrupts(uint16_t pinMask, const InterruptCallback &callback, uint8_t mode) {}

        inline  __attribute__((__always_inline__))
        void disableInterrupts(uint16_t pinMask) {}

        inline  __attribute__((__always_inline__))
        bool interruptsEnabled() {
            return false;
        }

        inline  __attribute__((__always_inline__))
        void interruptHandler() {}

        inline  __attribute__((__always_inline__))
        bool setInterruptFlag() {
            return true;
        }

    public:
        DDRType DDR;
        PINType PIN;
        PORTType PORT;
    };

}
