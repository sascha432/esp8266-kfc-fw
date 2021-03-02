/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#if IOT_DIMMER_MODULE_INTERFACE_UART
#include <SerialTwoWire.h>
#else
#include <Wire.h>
#endif

namespace Dimmer {

    #if IOT_DIMMER_MODULE_INTERFACE_UART
        using TwoWireBase = SerialTwoWire;
    #else
        using TwoWireBase = TwoWire;
    #endif

    class TwoWireEx : public TwoWireBase
    {
    public:
        using TwoWireBase::read;
        using TwoWireBase::write;

        template<class T>
        size_t read(T &data) {
            return readBytes(reinterpret_cast<uint8_t *>(&data), sizeof(data));
        }

        template<class T>
        size_t write(const T &data) {
            return write(reinterpret_cast<const uint8_t *>(&data), sizeof(data));
        }

        uint8_t endTransmission(uint8_t sendStop)
        {
            return TwoWireBase::endTransmission(sendStop);
        }

        uint8_t endTransmission() {
            return endTransmission(true);
        }


    #if IOT_DIMMER_MODULE_INTERFACE_UART

        TwoWireEx(Stream &stream);

        bool lock() {
            noInterrupts();
            if (_locked) {
                interrupts();
                __DBG_printf_E("wire locked");
                return false;
            }
            interrupts();
            _locked = true;
            return true;
        }

        void unlock() {
            noInterrupts();
            _locked = false;
            interrupts();
        }

    private:
        // static because the class is used with reinterpreting the Wire object
        static volatile bool _locked;
    #else

        using TwoWireBase::TwoWireBase;

        bool lock() {
            noInterrupts();
            return true;
        }

        void unlock() {
            interrupts();
            delay(1);
        }

    #endif
    };

}
