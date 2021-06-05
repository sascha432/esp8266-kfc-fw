/**
 * Author: sascha_lammers@gmx.de
 */

// A class that can enable performance optimization or increase CPU frequency
// Can be initiated on the stack to disable when poping the object
// currently only for ESP8266

#pragma once

#include <Arduino_compat.h>

#if defined(ESP8266) && SPEED_BOOSTER_ENABLED && (F_CPU == 160000000L)
#error SPEED_BOOSTER_ENABLED not supported with 160MHZ
#endif


// boost CPU speed
class SpeedBooster {
public:
    SpeedBooster(const SpeedBooster &helper) = delete;
    // auto enable/disable
    SpeedBooster(bool enable = true);
    ~SpeedBooster();

    // manual control
    static bool isEnabled();
    static void enable(bool value);
    static bool isAvailable();

#if SPEED_BOOSTER_ENABLED
private:
    static uint8_t _counter;
    bool _enabled;

    bool _autoEnable;
#endif
};
