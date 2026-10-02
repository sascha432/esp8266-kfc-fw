/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase.h"

#include <math.h>

namespace WeatherStation2 {

namespace MoonPhase {

static constexpr double kDegToRad = 0.017453292519943295769;
// 1 AU in km, used to compare the distance to the sun with the distance to the moon
static constexpr double kAstronomicalUnit = 149597870.7;
// Julian date of the unix epoch 1970-01-01 00:00 UT
static constexpr double kUnixEpochJulianDate = 2440587.5;
// mean phase of the lunation 0 (Meeus 49.1)
static constexpr double kMeanPhaseJde = 2451550.09766;

inline double _sinDeg(double degrees)
{
    return sin(degrees * kDegToRad);
}

inline double _cosDeg(double degrees)
{
    return cos(degrees * kDegToRad);
}

// normalizes to 0..360
inline double _normalize(double degrees)
{
    degrees = fmod(degrees, 360.0);
    return degrees < 0 ? degrees + 360.0 : degrees;
}

// 0..3 for negative lunation numbers too
inline uint8_t _quarter(int64_t index)
{
    auto quarter = static_cast<int32_t>(index % 4);
    return static_cast<uint8_t>(quarter < 0 ? quarter + 4 : quarter);
}

// ------------------------------------------------------------------------------------------
// Meeus chapter 47/48: position of the moon and the illuminated fraction
// ------------------------------------------------------------------------------------------

struct Position {
    double sunLongitude;
    double sunDistance;
    double longitude;
    double latitude;
    double distance;
};

static Position _position(double jd)
{
    const double t = (jd - kJ2000) / 36525.0;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double t4 = t3 * t;

    // --- the sun, Meeus chapter 25 (low precision, ~0.01 deg)
    const double sunMeanLongitude = _normalize(280.46646 + 36000.76983 * t + 0.0003032 * t2);
    const double sunMeanAnomaly = _normalize(357.52911 + 35999.05029 * t - 0.0001537 * t2);
    const double eccentricity = 0.016708634 - 0.000042037 * t - 0.0000001267 * t2;
    const double sunCenter =
        (1.914602 - 0.004817 * t - 0.000014 * t2) * _sinDeg(sunMeanAnomaly) +
        (0.019993 - 0.000101 * t) * _sinDeg(2 * sunMeanAnomaly) +
        0.000289 * _sinDeg(3 * sunMeanAnomaly);
    const double trueAnomaly = sunMeanAnomaly + sunCenter;

    // --- the moon, Meeus chapter 47
    const double l = _normalize(218.3164477 + 481267.88123421 * t - 0.0015786 * t2 + t3 / 538841.0 - t4 / 65194000.0);
    const double d = _normalize(297.8501921 + 445267.1114034 * t - 0.0018819 * t2 + t3 / 545868.0 - t4 / 113065000.0);
    const double m = _normalize(357.5291092 + 35999.0502909 * t - 0.0001536 * t2 + t3 / 24490000.0);
    const double mp = _normalize(134.9633964 + 477198.8675055 * t + 0.0087414 * t2 + t3 / 69699.0 - t4 / 14712000.0);
    const double f = _normalize(93.2720950 + 483202.0175233 * t - 0.0036539 * t2 - t3 / 3526000.0 + t4 / 863310000.0);
    const double e = 1.0 - 0.002516 * t - 0.0000074 * t2;

    // table 47.A, longitude. The coefficients are the table values, i.e. already in degrees
    // (table 47.A lists them in 1e-6 degrees). The terms below 0.0011 deg are omitted, the remaining
    // error is < 0.01 deg which is ~1.5% of the synodic month's 12.19 deg/day - plenty for a display
    double sumLongitude =
        +6.288774 * _sinDeg(mp)
        + 1.274027 * _sinDeg(2 * d - mp)
        + 0.658314 * _sinDeg(2 * d)
        + 0.213618 * _sinDeg(2 * mp)
        - 0.185116 * e * _sinDeg(m)
        - 0.114332 * _sinDeg(2 * f)
        + 0.058793 * _sinDeg(2 * d - 2 * mp)
        + 0.057066 * e * _sinDeg(2 * d - m - mp)
        + 0.053322 * _sinDeg(2 * d + mp)
        + 0.045758 * e * _sinDeg(2 * d - m)
        - 0.040923 * e * _sinDeg(m - mp)
        - 0.034720 * _sinDeg(d)
        - 0.030383 * e * _sinDeg(m + mp)
        + 0.015327 * _sinDeg(2 * d - 2 * f)
        - 0.012528 * _sinDeg(mp + 2 * f)
        + 0.010980 * _sinDeg(mp - 2 * f)
        + 0.010675 * _sinDeg(4 * d - mp)
        + 0.010034 * _sinDeg(3 * mp)
        + 0.008548 * _sinDeg(4 * d - 2 * mp)
        - 0.007888 * e * _sinDeg(2 * d + m - mp)
        - 0.006766 * e * _sinDeg(2 * d + m)
        - 0.005163 * _sinDeg(d - mp)
        + 0.004987 * e * _sinDeg(d + m)
        + 0.004036 * _sinDeg(2 * d - m + mp)
        + 0.003994 * _sinDeg(2 * d + 2 * mp)
        + 0.003861 * _sinDeg(4 * d)
        + 0.003665 * _sinDeg(2 * d - 3 * mp)
        - 0.002689 * e * _sinDeg(m - 2 * mp)
        - 0.002602 * _sinDeg(2 * d - mp + 2 * f)
        + 0.002390 * e * _sinDeg(2 * d - m - 2 * mp)
        - 0.002348 * _sinDeg(d + mp)
        + 0.002236 * e * _sinDeg(2 * d - 2 * m)
        - 0.002120 * e * _sinDeg(m + 2 * mp)
        - 0.002069 * e * e * _sinDeg(2 * m)
        + 0.002048 * e * _sinDeg(2 * d - 2 * m - mp)
        - 0.001773 * _sinDeg(2 * d + mp - 2 * f)
        - 0.001595 * _sinDeg(2 * d + 2 * f)
        + 0.001215 * e * _sinDeg(4 * d - m - mp)
        - 0.001110 * _sinDeg(2 * mp + 2 * f);

    // table 47.B, latitude, also in degrees
    double sumLatitude =
        +5.128122 * _sinDeg(f)
        + 0.280602 * _sinDeg(mp + f)
        + 0.277693 * _sinDeg(mp - f)
        + 0.173237 * _sinDeg(2 * d - f)
        + 0.055413 * _sinDeg(2 * d - mp + f)
        + 0.046271 * _sinDeg(2 * d - mp - f)
        + 0.032573 * _sinDeg(2 * d + f)
        + 0.017198 * _sinDeg(2 * mp + f)
        + 0.009266 * _sinDeg(2 * d + mp - f)
        + 0.008822 * _sinDeg(2 * mp - f)
        + 0.008216 * e * _sinDeg(m + f)
        + 0.004324 * _sinDeg(2 * d - 2 * mp - f)
        + 0.004200 * _sinDeg(2 * d + mp + f)
        + 0.003372 * _sinDeg(3 * mp + f)
        + 0.002472 * _sinDeg(2 * d - 2 * f)
        + 0.002222 * _sinDeg(2 * d + m - f)
        + 0.002072 * _sinDeg(2 * d - m - f);

    // table 47.A, distance (1e-3 km), only used to compute the phase angle
    double sumDistance =
        -20905355 * _cosDeg(mp)
        - 3699111 * _cosDeg(2 * d - mp)
        - 2955968 * _cosDeg(2 * d)
        - 569925 * _cosDeg(2 * mp)
        + 48888 * e * _cosDeg(m)
        - 3149 * _cosDeg(2 * f)
        + 246158 * _cosDeg(2 * d - 2 * mp)
        - 152138 * e * _cosDeg(2 * d - m - mp)
        - 170733 * _cosDeg(2 * d + mp)
        - 204586 * e * _cosDeg(2 * d - m)
        - 129620 * e * _cosDeg(m - mp)
        + 108743 * _cosDeg(d)
        + 104755 * e * _cosDeg(m + mp)
        + 10321 * _cosDeg(2 * d - 2 * f)
        + 79661 * e * _cosDeg(m - 2 * mp);

    Position position;
    position.sunLongitude = sunMeanLongitude + sunCenter;
    position.sunDistance = 1.000001018 * (1 - eccentricity * eccentricity) / (1 + eccentricity * _cosDeg(trueAnomaly));
    // the tables above are listed in degrees (table 47.A/47.B are in 1e-6 degrees, the values here
    // are already divided by 1e6), the distance table is in 0.001 km
    position.longitude = l + sumLongitude;
    position.latitude = sumLatitude;
    position.distance = 385000.56 + sumDistance / 1000.0;
    return position;
}

State calculate(time_t utc)
{
    const double jd = julianDate(utc);
    const auto position = _position(jd);

    // the elongation is the angle between the sun and the moon as seen from the earth
    const double elongation = _normalize(position.longitude - position.sunLongitude);

    State state;
    state.julianDate = jd;
    state.phase = elongation / 360.0;
    state.age = state.phase * kSynodicMonth;
    state.waxing = elongation < 180.0;

    // Meeus 48.3: the phase angle, using both distances and the latitude of the moon
    const double psi = acos(_cosDeg(position.latitude) * _cosDeg(elongation));
    const double sunDistance = position.sunDistance * kAstronomicalUnit;
    const double phaseAngle = atan2(sunDistance * sin(psi), position.distance - sunDistance * cos(psi));
    state.illumination = (1.0 + cos(phaseAngle)) * 0.5;

    return state;
}

// ------------------------------------------------------------------------------------------
// Meeus chapter 49: instants of the four phases
// ------------------------------------------------------------------------------------------

// Julian date (Dynamical Time) of a phase, index counts quarters: 0 = new, 1 = first quarter, ...
static double _phaseInstant(int64_t index)
{
    // the series is parameterized by the lunation number, 0.25 per quarter
    const double k = static_cast<double>(index) / 4.0;
    const double t = k / 1236.85;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double t4 = t3 * t;

    // mean phase (Meeus 49.1)
    double jde = kMeanPhaseJde + kSynodicMonth * k + 0.00015437 * t2 - 0.000000150 * t3 + 0.00000000073 * t4;

    const double e = 1.0 - 0.002516 * t - 0.0000074 * t2;
    const double m = _normalize(2.5534 + 29.10535670 * k - 0.0000014 * t2 - 0.00000011 * t3);
    const double mp = _normalize(201.5643 + 385.81693528 * k + 0.0107582 * t2 + 0.00001238 * t3 - 0.000000058 * t4);
    const double f = _normalize(160.7108 + 390.67050284 * k - 0.0016118 * t2 - 0.00000227 * t3 + 0.000000011 * t4);
    const double omega = _normalize(124.7746 - 1.56375588 * k + 0.0020672 * t2 + 0.00000215 * t3);

    const auto quarter = _quarter(index);

    if (quarter == 0 || quarter == 2) {
        // Meeus table 49.A: new moon and full moon, only the two largest terms differ
        jde += ((quarter == 0) ? -0.40720 : -0.40614) * _sinDeg(mp) +
               ((quarter == 0) ? 0.17241 : 0.17302) * e * _sinDeg(m) +
               0.01608 * _sinDeg(2 * mp) +
               0.01039 * _sinDeg(2 * f) +
               0.00739 * e * _sinDeg(mp - m) -
               0.00514 * e * _sinDeg(mp + m) +
               0.00208 * e * e * _sinDeg(2 * m) -
               0.00111 * _sinDeg(mp - 2 * f) -
               0.00057 * _sinDeg(mp + 2 * f) +
               0.00056 * e * _sinDeg(2 * mp + m) -
               0.00042 * _sinDeg(3 * mp) +
               0.00042 * e * _sinDeg(m + 2 * f) +
               0.00038 * e * _sinDeg(m - 2 * f) -
               0.00024 * e * _sinDeg(2 * mp - m) -
               0.00017 * _sinDeg(omega) -
               0.00007 * _sinDeg(mp + 2 * m) +
               0.00004 * _sinDeg(2 * mp - 2 * f) +
               0.00004 * _sinDeg(3 * m) +
               0.00003 * _sinDeg(mp + m - 2 * f) +
               0.00003 * _sinDeg(2 * mp + 2 * f) -
               0.00003 * _sinDeg(mp + m + 2 * f) +
               0.00003 * _sinDeg(mp - m + 2 * f) -
               0.00002 * _sinDeg(mp - m - 2 * f) -
               0.00002 * _sinDeg(3 * mp + m) +
               0.00002 * _sinDeg(4 * mp);
    }
    else {
        // Meeus table 49.B: first and last quarter
        jde += -0.62801 * _sinDeg(mp) +
               0.17172 * e * _sinDeg(m) -
               0.01183 * e * _sinDeg(mp + m) +
               0.00862 * _sinDeg(2 * mp) +
               0.00804 * _sinDeg(2 * f) +
               0.00454 * e * _sinDeg(mp - m) +
               0.00204 * e * e * _sinDeg(2 * m) -
               0.00180 * _sinDeg(mp - 2 * f) -
               0.00070 * _sinDeg(mp + 2 * f) -
               0.00040 * _sinDeg(3 * mp) -
               0.00034 * e * _sinDeg(2 * mp - m) +
               0.00032 * e * _sinDeg(m + 2 * f) +
               0.00032 * e * _sinDeg(m - 2 * f) -
               0.00028 * e * e * _sinDeg(mp + 2 * m) +
               0.00027 * e * _sinDeg(2 * mp + m) -
               0.00017 * _sinDeg(omega) -
               0.00005 * _sinDeg(mp - m - 2 * f) +
               0.00004 * _sinDeg(2 * mp + 2 * f) -
               0.00004 * _sinDeg(mp + m + 2 * f) +
               0.00004 * _sinDeg(mp - 2 * m) +
               0.00003 * _sinDeg(mp + m - 2 * f) +
               0.00003 * _sinDeg(3 * m) +
               0.00002 * _sinDeg(2 * mp - 2 * f) +
               0.00002 * _sinDeg(mp - m + 2 * f) -
               0.00002 * _sinDeg(3 * mp + m);

        // the quarter phases are shifted, the correction is added to the first and
        // subtracted from the last quarter (Meeus 49.4)
        const double correction = 0.00306 -
                                  0.00038 * e * _cosDeg(m) +
                                  0.00026 * _cosDeg(mp) -
                                  0.00002 * _cosDeg(mp - m) +
                                  0.00002 * _cosDeg(mp + m) +
                                  0.00002 * _cosDeg(2 * f);
        jde += (quarter == 1) ? correction : -correction;
    }

    // additional planetary corrections (Meeus table 49.C), they apply to all four phases
    jde += 0.000325 * _sinDeg(299.77 + 0.107408 * k - 0.009173 * t2) +
           0.000165 * _sinDeg(251.88 + 0.016321 * k) +
           0.000164 * _sinDeg(251.83 + 26.651886 * k) +
           0.000126 * _sinDeg(349.42 + 36.412478 * k) +
           0.000110 * _sinDeg(84.66 + 18.206239 * k) +
           0.000062 * _sinDeg(141.74 + 53.303771 * k) +
           0.000060 * _sinDeg(207.14 + 2.453732 * k) +
           0.000056 * _sinDeg(154.84 + 7.306860 * k) +
           0.000047 * _sinDeg(34.52 + 27.261239 * k) +
           0.000042 * _sinDeg(207.19 + 0.121824 * k) +
           0.000040 * _sinDeg(291.34 + 1.844379 * k) +
           0.000037 * _sinDeg(161.72 + 24.198154 * k) +
           0.000035 * _sinDeg(239.56 + 25.513099 * k) +
           0.000023 * _sinDeg(331.55 + 3.592518 * k);

    return jde;
}

// UT timestamp of a phase index
static time_t _phaseTime(int64_t index)
{
    // the series is in Dynamical Time, UT is behind by kDeltaT seconds
    return timeFromJulianDate(_phaseInstant(index) - kDeltaT / 86400.0);
}

// first quarter index whose phase instant is not before jd
static int64_t _firstIndex(double jd)
{
    auto index = static_cast<int64_t>(floor((jd - kMeanPhaseJde) / (kSynodicMonth / 4.0)));
    const double deltaT = kDeltaT / 86400.0;
    // the mean phase is off by up to ~0.6 days, walk to the first phase that really is in the future
    while (_phaseInstant(index) - deltaT <= jd) {
        index++;
    }
    while (index > 0 && _phaseInstant(index - 1) - deltaT > jd) {
        index--;
    }
    return index;
}

void nextPhases(time_t utc, PhaseTime out[4])
{
    auto index = _firstIndex(julianDate(utc));
    for (uint8_t i = 0; i < 4; i++, index++) {
        out[i].phase = static_cast<Phase>(_quarter(index));
        out[i].utc = _phaseTime(index);
    }
}

time_t nextPhase(time_t utc, Phase phase)
{
    auto index = _firstIndex(julianDate(utc));
    const auto quarter = static_cast<uint8_t>(phase);
    // at most four steps, the requested phase is always within the next lunation
    while (_quarter(index) != quarter) {
        index++;
    }
    return _phaseTime(index);
}
double julianDate(time_t utc)
{
    return kUnixEpochJulianDate + static_cast<double>(utc) / 86400.0;
}

time_t timeFromJulianDate(double jd)
{
    return static_cast<time_t>((jd - kUnixEpochJulianDate) * 86400.0 + 0.5);
}

// ------------------------------------------------------------------------------------------
// names
// ------------------------------------------------------------------------------------------

const __FlashStringHelper *name(double phase)
{
    switch (static_cast<uint8_t>(floor(_normalize(phase * 360.0) / 45.0 + 0.5)) & 7) {
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
    default: // 7
        return F("Waning Crescent");
    }
}

const __FlashStringHelper *quarterName(Phase phase)
{
    switch (static_cast<uint8_t>(phase) & 3) {
    case 0:
        return F("New Moon");
    case 1:
        return F("First Quarter");
    case 2:
        return F("Full Moon");
    default: // 3 = LAST_QUARTER
        return F("Last Quarter");
    }
}

} // namespace MoonPhase

} // namespace WeatherStation2
