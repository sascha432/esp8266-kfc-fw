/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>

#ifndef DEBUG_MOON_PHASE
#   define DEBUG_MOON_PHASE 0
#endif

struct MoonPhaseType
{
    // number of days of a synodic month
    static constexpr auto kSynMonth = 29.53058868;

    // show full moon etc. 10 hours before and after it actually occurs
    static constexpr auto kWindowInHours = 10;
    // calculate the pPhase value for kWindowInHours
    static constexpr auto kWindowAsPhaseValue = 1 / ((kSynMonth * 24.0) / kWindowInHours);

    static const __FlashStringHelper *moonPhaseName(uint8_t moonPhase);
    static const __FlashStringHelper *moonPhaseHuntName(uint8_t phaseHunt);

    double iPhase;
    double pPhase;
    double mAge;
    double earthDistance;
    const __FlashStringHelper *phaseName;
    time_t unixTime;
    double julianTime;
    char moonPhaseFont;

    MoonPhaseType() : mAge(NAN), phaseName(nullptr) {
    }

    bool isValid(time_t curTime) const {
        return !isnan(mAge) && (abs(curTime - unixTime) <= 60);
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
        return phaseName;
    }

    // returns the number for moonPhaseHuntName() or -1 if outside the window
    int8_t moonPhaseHuntNum() const {
        for(uint8_t i = 1; i <= 4; i++) {
            if (abs(pPhase - (i / 4.0)) < kWindowAsPhaseValue) {
                return i % 4;
            }
        }
        return -1;
    }

    String moonPhaseHuntNumDebug() const {
        int8_t phaseNum = -1;
        float phase = NAN;
        float distance = 255;
        for(uint8_t i = 1; i <= 4; i++) {
            auto d = abs(pPhase - (i / 4.0));
            if (d < distance) {
                distance = d;
                phase = i / 4.0;
                phaseNum = i % 4;
            }
        }
        return PrintString(F("pPhase=%.4f phase=%.2f/%d(%s) range=%f/%f match=%u"), pPhase, phase, phaseNum, moonPhaseHuntName(phaseNum), distance, kWindowAsPhaseValue, distance < kWindowAsPhaseValue);
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

#if DEBUG_MOON_PHASE
    void startMoonDebug();
#endif
