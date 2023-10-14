/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#if IOT_DIMMER_MODULE_INTERFACE_UART
#    include <SerialTwoWire.h>
#else
#    ifndef DISABLE_TWO_WIRE
#        include <Wire.h>
#    endif
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
            auto res = TwoWireBase::endTransmission(sendStop);
            delay(1);
            return res;
        }

        uint8_t endTransmission() {
            return endTransmission(true);
        }


    #if IOT_DIMMER_MODULE_INTERFACE_UART

        TwoWireEx(Stream &stream);

        bool lock() {
            InterruptLock lock;
            if (_locked) {
                __LDBG_printf_E("wire locked");
                return false;
            }
            _locked = true;
            return true;
        }

        void unlock() {
            InterruptLock lock;
            _locked = false;
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
