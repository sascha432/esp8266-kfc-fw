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

        int analogRead(uint8_t pin);
        void analogWrite(uint8_t pin, int val);

        void writePort(uint8_t value);
        uint8_t readPort();

        inline  __attribute__((__always_inline__))
        void writePortA(uint8_t value) {
            writePort(value);
        }

        inline  __attribute__((__always_inline__))
        uint8_t readPortA() {
            return readPort();
        }

        void analogReference(uint8_t mode) {}
        void analogWriteFreq(uint32_t freq) {}

        void enableInterrupts(uint8_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode);
        void disableInterrupts(uint8_t pinMask);
        bool interruptsEnabled();

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

    protected:
        uint8_t _intPins;
        uint8_t _intState;
        uint16_t _intMode;
    };

}
