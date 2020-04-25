/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>

namespace SaveCrash {

    uint8_t getCrashCounter();
    void removeCrashCounterAndSafeMode();
    void removeCrashCounter();

};

#if DEBUG_HAVE_SAVECRASH

#include <EspSaveCrash.h>

extern EspSaveCrash espSaveCrash;

namespace SaveCrash {
    void installRemoveCrashCounter(uint32_t delay_seconds);
    void installSafeCrashTimer(uint32_t delay_seconds);
}

#endif
