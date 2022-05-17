/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_MOON_PHASE
#   define DEBUG_MOON_PHASE 0
#endif

constexpr auto kMoonDay = 29.53058868;

struct MoonPhaseType
{
    double pPhase;
    double mAge;
    char moonPhaseFont;
    time_t uTime; // unixtime
    double jTime;

    MoonPhaseType() : mAge(NAN) {
    }

    bool isValid(time_t curTime) const {
        return !isnan(mAge) && abs(curTime - uTime) <= 60;
    }

    void invalidate() {
        mAge = NAN;
    }

    uint8_t moonDay() const {
        return mAge;
    }
};

struct MoonPhasesType
{
    // 0 new moon
    // 2 full moon
    // ...
    time_t _timestamps[5]; // unixtime

    MoonPhasesType() : _timestamps{}
    {}
};

MoonPhasesType calcMoonPhases(time_t unixtime);

MoonPhaseType calcMoon(time_t time);

const __FlashStringHelper *moonPhaseName(uint8_t moonPhase);

inline const __FlashStringHelper *moonPhaseName(double moonPhase)
{
    return moonPhaseName(static_cast<uint8_t>(moonPhase * 8));
}
