/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// Moon phase calculation of the weather station 2.x plugin.
//
// This file is self-contained on purpose: <stdint.h>, <time.h> and Arduino_compat.h for the type of
// the PROGMEM strings it returns. No LVGL, no KFC plugin and no graphics dependency, so the module
// can be reused by any other plugin.
//
// The algorithms are from Jean Meeus, "Astronomical Algorithms" (2nd edition):
//   - chapter 47: the moon's ecliptic longitude/latitude and the distance to the earth
//                 (truncated periodic series, ~0.01 deg for the longitude)
//   - chapter 48: illuminated fraction of the moon's disc (phase angle from the elongation)
//   - chapter 49: instants of new moon, first quarter, full moon and last quarter
//                 (periodic series with the planetary corrections A1..A14, accurate to ~1 minute)
//
// Everything is UTC. The result of the chapter 49 series is in Dynamical Time, kDeltaT converts it
// to UT. Formatting to the local time of the location (or of the device) is up to the caller.
//

#include <stdint.h>
#include <time.h>

// PROGMEM strings: __FlashStringHelper, F(), PSTR(), PROGMEM
#include <Arduino_compat.h>

namespace WeatherStation2 {

namespace MoonPhase {

// length of a synodic month in days (Meeus 49.1, "kDeltaT" is only used for the phase instants)
static constexpr double kSynodicMonth = 29.530588861;

// difference between Dynamical Time (TD) and UT in seconds, ~69s in 2026 (Meeus 49, chapter 10)
static constexpr double kDeltaT = 69.0;

// Julian date (UT) of 2000-01-01 12:00
static constexpr double kJ2000 = 2451545.0;

// the four phases of a lunation, the value is the quarter index of the chapter 49 series
enum class Phase : uint8_t {
    NEW_MOON = 0,
    FIRST_QUARTER = 1,
    FULL_MOON = 2,
    LAST_QUARTER = 3,
};

// state of the moon at one instant
struct State {
    // Julian date (UT) the state was calculated for
    double julianDate;
    // days since the last new moon, 0 .. kSynodicMonth
    double age;
    // illuminated fraction of the disc, 0 = new, 1 = full
    double illumination;
    // 0 = new, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter
    double phase;
    // true while the illuminated part grows
    bool waxing;
};

// instant of one of the four phases
struct PhaseTime {
    Phase phase;
    // UTC timestamp of the instant
    time_t utc;
};

// state of the moon for a UTC timestamp
State calculate(time_t utc);

// the next four phases after utc, in chronological order
void nextPhases(time_t utc, PhaseTime out[4]);

// the next occurrence of one specific phase after utc
time_t nextPhase(time_t utc, Phase phase);

// Julian date of a UTC timestamp and the inverse (both are UT based)
double julianDate(time_t utc);
time_t timeFromJulianDate(double jd);

// the eight phase names, phase is the value of State::phase (0..1). The returned string is stored
// in flash (PROGMEM), it must be read with a flash aware reader (String, Print::print, ...)
const __FlashStringHelper *name(double phase);

// the name of one of the four phases, stored in flash like name()
const __FlashStringHelper *quarterName(Phase phase);

} // namespace MoonPhase

} // namespace WeatherStation2
