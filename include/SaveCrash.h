/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>

namespace SaveCrash {

    uint8_t getCrashCounter();
    void removeCrashCounterAndSafeMode();
    void removeCrashCounter(); // calling SPIFFS.begin()
    void installRemoveCrashCounter(uint32_t delay_seconds);

};

#if DEBUG_HAVE_SAVECRASH

#ifndef SAVECRASH_MAX_DUMPS
#define SAVECRASH_MAX_DUMPS             100
#endif

#include <EspSaveCrash.h>

extern EspSaveCrash espSaveCrash;

namespace SaveCrash {
    void installSafeCrashTimer(uint32_t delay_seconds);
}

#endif
