/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "pin_monitor.h"

namespace PinMonitor {

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
