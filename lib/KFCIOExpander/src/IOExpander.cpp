    /**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "IOExpander.h"

#if HAVE_IOEXPANDER

namespace IOExpander {

    #if IOEXPANDER_DEVICE_CONFIG

        #define LT <
        #define GT >

        ConfigIterator<IOEXPANDER_DEVICE_CONFIG> config;

        #undef LT
        #undef GT

    #else

    ConfigEndIterator config;

    #endif

    void IRAM_ATTR __interruptHandler(void *arg)
    {
        ::printf("interrupt %p\n", arg);
        config._setInterruptFlagRecursive(arg);
    }

    void scanBus(Print &output, TwoWire &wire, uint8_t fromAddress, uint8_t toAddress, uint32_t delayMillis)
    {
        output.printf_P(PSTR("scanning address range 0x%02x-0x%02x:\n"), fromAddress, toAddress);
        for(uint8_t address = fromAddress; address <= toAddress; address++) {
            wire.beginTransmission(address);
            uint8_t error = wire.endTransmission(true);
            if (error == 0) {
                output.printf_P(PSTR("device @ 0x%02x\n"), address);
            }
            delay(delayMillis);
        }
    }

    #if !HAVE_KFC_FIRMWARE_VERSION

        void ___DBG_printf(const char *msg, ...)
        {
            va_list arg;
            va_start(arg, msg);
            char temp[8];
            size_t len = vsnprintf(temp, sizeof(temp) - 1, msg, arg);
            va_end(arg);
            Serial.write(temp, len);
        }

    #endif

}

#endif
