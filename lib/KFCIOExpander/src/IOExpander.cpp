    /**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "IOExpander.h"

#if HAVE_IOEXPANDER

namespace IOExpander {

    #define LT <
    #define GT >

    ConfigIterator<IOEXPANDER_DEVICE_CONFIG> config;

    #undef LT
    #undef GT

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

}

#endif
