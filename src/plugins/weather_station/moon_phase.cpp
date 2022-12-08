/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase.h"
#include "moon/moontool.h"

static MoonPhaseType __moonCache[2];

const __FlashStringHelper *moonPhaseName(uint8_t moonPhase)
{
    switch (moonPhase % 4) {
        case 0:
            return F("Waxing Crescent");
        case 1:
            return F("Waxing Gibbous");
        case 2:
            return F("Waning Gibbous");
        case 3:
            return F("Waning Crescent");
    }
    __builtin_unreachable();
}

const __FlashStringHelper *moonPhaseHuntName(uint8_t phaseHunt)
{
    switch (phaseHunt % 4) {
        case 0:
            return F("New Moon");
        case 1:
            return F("First Quarter");
        case 2:
            return F("Full Moon");
        case 3:
            return F("Third Quarter");
    }
    __builtin_unreachable();
}

MoonPhasesType calcMoonPhases(time_t unixtime)
{
    struct tm *cur_gmt_time = gmtime(&unixtime);
    double cur_julian_time = julianTime(cur_gmt_time);

    double results[5];
    phaseHunt(cur_julian_time, results);

    MoonPhasesType result;
    for(uint8_t i = 0; i < 5; i++) {
        result._timestamps[i] = jToUnixtime(results[i]);
    }

    return result;
}

time_t getUnixtimeForCalcMoon()
{
    time_t unixtime;
    return getUnixtimeForCalcMoon(time(&unixtime));
}

time_t getUnixtimeForCalcMoon(time_t unixtime)
{
    unixtime -= 86400 + 1;
    auto tm = localtime(&unixtime);
    return mktime(tm);
}

MoonPhaseType calcMoon(time_t unixtime, bool runPhaseHunt)
{
    auto &_moonCache = __moonCache[runPhaseHunt ? 1 : 0];

    // check if we have valid data
    if (!_moonCache.isValid(unixtime)) {

        struct tm *cur_gmt_time = gmtime(&unixtime);
        double cur_julian_time = julianTime(cur_gmt_time);

        // run phase hunt
        int8_t overridepPhase = -1;
        if (runPhaseHunt) {
            double results[5];
            phaseHunt(cur_julian_time, results);
            auto unixtime2 = unixtime + 86400 - 1;
            auto today_yday = localtime(&unixtime2)->tm_yday + 1;
            for(int8_t i = 0; i < 5; i++) {
                auto time = jToUnixtime(results[i]);
                auto lt_yday = localtime(&time)->tm_yday;
                if (lt_yday == today_yday) {
                    overridepPhase = i;
                    break;
                }
            }
        }

        phaseShort(cur_julian_time, _moonCache);

        _moonCache.unixTime = unixtime;
        _moonCache.julianTime = cur_julian_time;

        if (runPhaseHunt) {
            if (overridepPhase >= 0) {
                _moonCache.phaseName = moonPhaseHuntName(overridepPhase); // today is new moon, first quarter, full moon or last quarter
            }
            else {
                _moonCache.phaseName = moonPhaseName(_moonCache.pPhase * 4.0);
            }
        }
        else {
            _moonCache.phaseName = moonPhaseHuntName(_moonCache.pPhase * 4.0);
        }

        // convert to letter for moon_phases font
        // https://www.dafont.com/moon-phases.font
        _moonCache.moonPhaseFont = 'A' + ((static_cast<uint8_t>(_moonCache.pPhase * 26) + 13) % 26);
        // _moonCache.moonPhaseFont = 'a' + ((static_cast<uint8_t>(pphase * 26) + 13) % 26);
    }
    return _moonCache;
}
