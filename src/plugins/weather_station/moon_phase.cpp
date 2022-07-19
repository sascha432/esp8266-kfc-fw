/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase.h"
#include "moon/moontool.h"

static MoonPhaseType _moonCache;

const __FlashStringHelper *moonPhaseName(uint8_t moonPhase)
{
    switch(moonPhase % 8) {
        case 0:
            return F("New Moon");
        case 1:
            return F("Waxing Crescent");
        case 2:
            return F("First Quarter");
        case 3:
            return F("Waxing Gibbous");
        case 4:
            return F("Full Moon");
        case 5:
            return F("Waning Gibbous");
        case 6:
            return F("Third Quarter");
        case 7:
            return F("Waning Crescent");
    }
    return nullptr;
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
    unixtime -= 86400 - 1;
    auto tm = localtime(&unixtime);
    // tm->tm_hour = 0;
    // tm->tm_min = 0;
    // tm->tm_sec = 0;
    return mktime(tm);
}

MoonPhaseType calcMoon(time_t unixtime)
{
    // check if we have valid data
    if (!_moonCache.isValid(unixtime)) {

        struct tm *cur_gmt_time = gmtime(&unixtime);
        double cur_julian_time = julianTime(cur_gmt_time);

        phaseShort(cur_julian_time, _moonCache);

        _moonCache.unixTime = unixtime;
        _moonCache.julianTime = cur_julian_time;

        // convert to letter for moon_phases font
        // https://www.dafont.com/moon-phases.font
        _moonCache.moonPhaseFont = 'A' + ((static_cast<uint8_t>(_moonCache.pPhase * 26) + 13) % 26);
        // _moonCache.moonPhaseFont = 'a' + ((static_cast<uint8_t>(pphase * 26) + 13) % 26);
    }
    return _moonCache;
}
