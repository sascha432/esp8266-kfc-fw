/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase.h"

extern "C" {
    #include "moon/julian.h"
    #include "moon/moontool.h"
}

// float unixTimeToJulianDate(time_t time)
// {
//     return (time / 86400.0) + 2440587.5;
// }

static time_t utime(double jTime) {
    return (jTime - 2440587.5) * 86400;
}

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
    double cur_julian_time = jtime(cur_gmt_time);

    double results[5];
    phasehunt(cur_julian_time, results);

    MoonPhasesType result;
    for(uint8_t i = 0; i < 5; i++) {
        result._timestamps[i] = utime(results[i]);
    }

    return result;
}

MoonPhaseType calcMoon(time_t unixtime)
{
    // check if we have valid data
    if (!_moonCache.isValid(unixtime)) {

        struct tm *cur_gmt_time = gmtime(&unixtime);
        double cur_julian_time = jtime(cur_gmt_time);

        double tmp;
        _moonCache.pPhase = phase_short(cur_julian_time, &tmp, &_moonCache.mAge);

        _moonCache.uTime = unixtime;
        _moonCache.jTime = cur_julian_time;

        // convert to letter for moon_phases font
        // https://www.dafont.com/moon-phases.font
        _moonCache.moonPhaseFont = 'A' + ((static_cast<uint8_t>(_moonCache.pPhase * 26) + 13) % 26);
        // _moonCache.moonPhaseFont = 'a' + ((static_cast<uint8_t>(pphase * 26) + 13) % 26);
    }
    return _moonCache;
}
