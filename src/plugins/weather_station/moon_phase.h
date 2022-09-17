/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>


#ifndef DEBUG_MOON_PHASE
#   define DEBUG_MOON_PHASE 0
#endif

const __FlashStringHelper *moonPhaseName(uint8_t moonPhase, uint8_t ofs);

inline const __FlashStringHelper *moonPhaseName(double moonPhase)
{
    return moonPhaseName(static_cast<uint8_t>(moonPhase * 8), 1);
}

inline const __FlashStringHelper *moonPhaseNameNoOffset(double moonPhase)
{
    return moonPhaseName(static_cast<uint8_t>(moonPhase * 8), 0);
}

struct MoonPhaseType
{
    static constexpr auto kMoonDay = 29.53058868;

    double iPhase;
    double pPhase;
    double mAge;
    char moonPhaseFont;
    time_t unixTime;
    double julianTime;

    MoonPhaseType() : mAge(NAN) {
    }

    bool isValid(time_t curTime) const {
        return !isnan(mAge) && abs(curTime - unixTime) <= 60;
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
        return moonDayDouble() == 1.0 ? F("day") : F("days");
    }

    const __FlashStringHelper *moonPhaseName() const {
        return ::moonPhaseName(pPhase);
    }

    const __FlashStringHelper *moonPhaseNameNoOffset() const {
        return ::moonPhaseNameNoOffset(pPhase);
    }

    double moonPhaseIlluminationPct() const {
        return moonPhaseIllumination() * 100.0;
    }

    double moonPhaseIllumination() const {
        return iPhase;
    }
};

struct MoonPhasesType
{
    // 0 new moon
    // 1 first quarter
    // 2 full moon
    // 3 third quarter
    // 4 next new moon
    time_t _timestamps[5]; // unixtime

    MoonPhasesType() : _timestamps{}
    {}

    time_t *begin() {
        return &_timestamps[0];
    }

    time_t *end() {
        return &_timestamps[5];
    }
};

MoonPhasesType calcMoonPhases(time_t unixtime);
time_t getUnixtimeForCalcMoon();
time_t getUnixtimeForCalcMoon(time_t unixtime);
MoonPhaseType calcMoon(time_t time);
