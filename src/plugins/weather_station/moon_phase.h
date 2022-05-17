/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_MOON_PHASE
#   define DEBUG_MOON_PHASE 0
#endif

const __FlashStringHelper *moonPhaseName(uint8_t moonPhase);

inline const __FlashStringHelper *moonPhaseName(double moonPhase)
{
    return moonPhaseName(static_cast<uint8_t>(moonPhase * 8));
}

struct MoonPhaseType
{
    static constexpr auto kMoonDay = 29.53058868;
    static constexpr auto kQuarterMoonDay = 29.53058868 / 4.0;
    static constexpr auto kMoonDayToPct = 29.53058868 / 2.0;

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

    double moonDayDouble() const {
        return mAge;
    }

    const __FlashStringHelper *moonDayDescr() const {
        return moonDay() == 1 ? F("day") : F("days");
    }

    const __FlashStringHelper *moonPhaseName() const {
        return ::moonPhaseName(kMoonDay / mAge);
    }

    double moonAgePct() const {
        auto age = mAge;
        while(age > kQuarterMoonDay) {
            age -= kQuarterMoonDay;
        }
        return 100 - (kMoonDayToPct / age);
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
