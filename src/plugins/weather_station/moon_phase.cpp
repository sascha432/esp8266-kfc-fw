/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <Scheduler.h>
#include <time.h>
#include "moon_phase.h"
#include "moon/moontool.h"

#if DEBUG_IOT_WEATHER_STATION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

static MoonPhaseType _moonCache;

const __FlashStringHelper *MoonPhaseType::moonPhaseName(uint8_t moonPhase)
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

const __FlashStringHelper *MoonPhaseType::moonPhaseHuntName(uint8_t phaseHunt)
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

MoonPhaseType calcMoon(time_t unixtime)
{
    // check if we have valid data
    if (!_moonCache.isValid(unixtime)) {

        struct tm *cur_gmt_time = gmtime(&unixtime);
        double cur_julian_time = julianTime(cur_gmt_time);

        phaseShort(cur_julian_time, _moonCache);

        _moonCache.unixTime = unixtime;
        _moonCache.julianTime = cur_julian_time;

        auto phase = _moonCache.moonPhaseHuntNum();
        _moonCache.phaseName = (phase == -1) ? MoonPhaseType::moonPhaseName(_moonCache.pPhase * 4) : MoonPhaseType::moonPhaseHuntName(phase);

        // convert to letter for moon_phases font
        // https://www.dafont.com/moon-phases.font
        _moonCache.moonPhaseFont = 'A' + ((static_cast<uint8_t>(_moonCache.pPhase * 26) + 13) % 26);
    }
    return _moonCache;
}

#if DEBUG_MOON_PHASE

static void _debugMoonToFile(Stream &file)
{
    PrintString timeStr;
    auto now = time(nullptr);
    auto moon = calcMoon(getUnixtimeForCalcMoon());
    timeStr.strftime(FSPGM(strftime_date_time_zone), now);
    file.print(timeStr);
    file.printf_P(PSTR(" phase=%s hunt=%d %s"), moon.moonPhaseName(), moon.moonPhaseHuntNum(), moon.moonPhaseHuntNumDebug().c_str());
    file.printf_P(PSTR(" iPhase=%.3f%% mAge=%.3f earthDist=%.0f\n"), moon.moonPhaseIlluminationPct(), moon.mAge, moon.earthDistance);
}

void startMoonDebug()
{
    _debugMoonToFile(DEBUG_OUTPUT);
    _Scheduler.add(Event::minutes(5), true, [](Event::CallbackTimerPtr timer) {
        _debugMoonToFile(DEBUG_OUTPUT);
    });

    auto delay = time(nullptr) % (240 * 60);
    __LDBG_printf("delay=%u", delay);
    _Scheduler.add(Event::seconds(delay), true, [](Event::CallbackTimerPtr timer) {
        timer->updateInterval(Event::minutes(240));
        auto file = KFCFS.open(F("/.logs/moon_phase.log"), FileOpenMode::appendplus);
        if (file) {
            _debugMoonToFile(file);
        }
    });
}

#endif
