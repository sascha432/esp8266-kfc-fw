/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "pin_monitor.h"

namespace PinMonitor {

    const __FlashStringHelper *getActiveStateTypeStr(ActiveStateType type)
    {
        switch(type) {
            case ActiveStateType::ACTIVE_HIGH:
                return F("High");
            case ActiveStateType::ACTIVE_LOW:
                return F("LOW");
        }
        __builtin_unreachable();
    }

    const __FlashStringHelper *getPinModeStr(uint8_t mode)
    {
        switch(mode) {
            case INPUT:
                return F("INPUT");
            case INPUT_PULLUP:
                return F("INPUT_PULLUP");
            case OUTPUT:
                return F("OUTPUT");
            #ifdef INPUT_PULLDOWN_16
                case INPUT_PULLDOWN_16:
                    return F("INPUT_PULLUP");
            #endif
            #ifdef OUTPUT_OPEN_DRAIN
               	case OUTPUT_OPEN_DRAIN:
                    return F("OUTPUT_OPEN_DRAIN");
            #endif
            #ifdef WAKEUP_PULLDOWN
                case WAKEUP_PULLDOWN:
                    return F("WAKEUP_PULLDOWN");
            #endif
        }
        return F("<Unknown>");
    }

    const __FlashStringHelper *getHardwarePinTypeStr(HardwarePinType type)
    {
        switch(type) {
            case HardwarePinType::SIMPLE:
                return F("Simple Pin");
            case HardwarePinType::DEBOUNCE:
                return F("Debounced Button");
            case HardwarePinType::ROTARY:
                return F("Rotary Encoder");
            case HardwarePinType::BASE:
            case HardwarePinType::NONE:
                break;
        }
        return F("<Unknown>");
    }

}
