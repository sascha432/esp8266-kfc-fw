/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase.h"

// TODO
// there seems to be an issue with the name of the moon phase changing a couple hours
// before the actual change, but the image shows the correct phase
//
// there is a couple different values for moon cycle which might cause the
// issue. could be rounding issues or the other two constants in unixTimeToJulianDate()
// and calcMoon()
// MOON_CYCLE = 29.53058868
// MOON_CYCLE = 29.5305888531
// MOON_CYCLE = 29.53058770576
//
// test script not finished
// src\plugins\weather_station\moon_phase.py
//
// Moon: 1.1% - Waning Crescent, displaying New Moon
// Current Time:	Jan 31, 2022 at 1:40:44 am
// Moon Phase Tonight:	Waning Crescent
// New Moon:	Jan 31, 2022 at 9:46 pm (Next Phase)

float unixTimeToJulianDate(time_t time)
{
    return (time / 86400.0) + 2440587.5;
}

void calcMoon(time_t time, float &moonDay, uint8_t &moonPhase,char &moonPhaseFont, bool upperCase)
{
    const float moonCycle = 29.53;
    float value = (unixTimeToJulianDate(time) - 2244116.75) / moonCycle;
    value -= floor(value);
    moonDay = value * moonCycle; // convert to days
    moonPhase = (uint8_t)((value * 8) + 0.5) & 0x07; // convert to moon phase (0-7)
    // convert to letter for moon_phases font
    // https://www.dafont.com/moon-phases.font
    if (upperCase) {
        moonPhaseFont = 'A' + ((uint8_t)(value * 26) % 26);
    }
    else {
        moonPhaseFont = 'a' + ((uint8_t)((value * 26) + 13) % 26);
    }
}

static PGM_P moon_phases[] PROGMEM = {
    PSPGM(moon_phase_0, "Full Moon"),
    PSPGM(moon_phase_1, "Waning Gibbous"),
    PSPGM(moon_phase_2, "Last Quarter"),
    PSPGM(moon_phase_3, "Old Crescent"),
    PSPGM(moon_phase_4, "New Moon"),
    PSPGM(moon_phase_5, "New Crescent"),
    PSPGM(moon_phase_6, "First Quarter"),
    PSPGM(moon_phase_7, "Waxing Gibbous")
};

const __FlashStringHelper *moonPhaseName(uint8_t moonPhase)
{
    return reinterpret_cast<const __FlashStringHelper *>(moon_phases[moonPhase & 0x07]);
}
