/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase.h"

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
