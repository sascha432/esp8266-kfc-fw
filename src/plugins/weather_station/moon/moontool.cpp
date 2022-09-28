/**
 * Modified by: sascha_lammers@gmx.de
 */

//
// https://www.fourmilab.ch/moontoolw/
//
// Moontool was originally written for the Sun Workstation under the SunView graphical user interface by John Walker in December of 1987. The program was posted to the Usenet news group comp.sources.unix in June 1988 and was subsequently widely distributed within the Sun user community. As the original posting began, “What good's a Sun without a Moon?”.
//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "moontool.h"
#include "../moon_phase.h"

#ifndef _MSC_VER
#pragma GCC push_options
#pragma GCC optimize ("Os")
#endif

/*  Astronomical constants  */

/* 1980 January 0.0 */
static constexpr auto kEpoch = 2444237.5;

/*  Constants defining the Sun's apparent orbit  */

#define elonge    278.833540 /* Ecliptic longitude of the Sun at epoch 1980.0 */
#define elongp    282.596403 /* Ecliptic longitude of the Sun at perigee */
#define eccent    0.016718 /* Eccentricity of Earth's orbit */
#define sunsmax   1.495985e8 /* Semi-major axis of Earth's orbit, km */
#define sunangsiz 0.533128 /* Sun's angular size, degrees, at semi-major axis distance */

/*  Elements of the Moon's orbit, epoch 1980.0  */

#define mmlong    64.975464 /* Moon's mean longitude at the epoch */
#define mmlongp   349.383063 /* Mean longitude of the perigee at the epoch */
#define mlnode    151.950429 /* Mean longitude of the node at the epoch */
#define minc      5.145396 /* Inclination of the Moon's orbit */
#define mecc      0.054900 /* Eccentricity of the Moon's orbit */
#define mangsiz   0.5181 /* Moon's angular size at distance a from Earth */
#define msmax     384401.0 /* Semi-major axis of Moon's orbit in km */
#define mparallax 0.9507 /* Parallax at distance a from Earth */
#define synmonth  29.53058868 /* Synodic month (new Moon to new Moon) */
#define lunatbase 2423436.0 /* Base date for E. W. Brown's numbered series of lunations (1923 January 16) */

/* Assume not near black hole nor in Tennessee */
#ifndef PI
static constexpr auto PI = 3.14159265358979323846;
#endif
static constexpr auto kToRad = PI / 180.0;
static constexpr auto kToDeg = 180.0 / PI;

/*  Handy mathematical functions  */

static double fixAngle(double a)
{
    return (a - (360.0 * (floor(a / 360.0))));
}

// Deg -> Rad
static double toRad(double d)
{
    return d * kToRad;
}

// Rad -> Deg
static double toDeg(double d)
{
    return d * kToDeg;
}

// Sin from deg
static double dsin(double x)
{
    return sin(toRad(x));
}

// Cos from deg
static double dcos(double x)
{
    return cos(toRad(x));
}

/*  MEANPHASE  --  Calculates  time  of  the mean new Moon for a given
           base date.  This argument K to this function is the
           precomputed synodic month index, given by:

              K = (year - 1900) * 12.3685

           where year is expressed as a year and fractional year.  */

double meanPhase(double sdate, double k)
{
    /* Time in Julian centuries from 1900 January 0.5 */
    auto t = (sdate - 2415020.0) / 36525;
    auto t2 = t * t; /* Square for frequent use */
    auto t3 = t2 * t; /* Cube for frequent use */

    auto nt1 = 2415020.75933 + synmonth * k
        + 0.0001178 * t2
        - 0.000000155 * t3
        + 0.00033 * dsin(166.56 + 132.87 * t - 0.009173 * t2);

    return nt1;
}

/*  TRUEPHASE  --  Given a K value used to determine the mean phase of
           the new moon, and a phase selector (0.0, 0.25, 0.5,
           0.75), obtain the true, corrected phase time.  */

double truePhase(double k, double phase)
{
    k += phase; /* Add phase to new moon time */
    auto t = k / 1236.85; /* Time in Julian centuries from 1900 January 0.5 */
    auto t2 = t * t; /* Square for frequent use */
    auto t3 = t2 * t; /* Cube for frequent use */

    /* Mean time of phase */
    auto pt = 2415020.75933 + synmonth * k + 0.0001178 * t2 - 0.000000155 * t3 + 0.00033 * dsin(166.56 + 132.87 * t - 0.009173 * t2);

    /* Sun's mean anomaly */
    auto m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3;

    /* Moon's mean anomaly */
    auto mprime = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3;

    /* Moon's argument of latitude */
    auto f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3;

    if ((phase < 0.01) || (std::abs(phase - 0.5) < 0.01)) {

        /* Corrections for New and Full Moon */

        pt += (0.1734 - 0.000393 * t) * dsin(m)
            + 0.0021 * dsin(2 * m)
            - 0.4068 * dsin(mprime)
            + 0.0161 * dsin(2 * mprime)
            - 0.0004 * dsin(3 * mprime)
            + 0.0104 * dsin(2 * f)
            - 0.0051 * dsin(m + mprime)
            - 0.0074 * dsin(m - mprime)
            + 0.0004 * dsin(2 * f + m)
            - 0.0004 * dsin(2 * f - m)
            - 0.0006 * dsin(2 * f + mprime)
            + 0.0010 * dsin(2 * f - mprime)
            + 0.0005 * dsin(m + 2 * mprime);
    }
    else if ((std::abs(phase - 0.25) < 0.01 || (std::abs(phase - 0.75) < 0.01))) {
        pt += (0.1721 - 0.0004 * t) * dsin(m)
            + 0.0021 * dsin(2 * m)
            - 0.6280 * dsin(mprime)
            + 0.0089 * dsin(2 * mprime)
            - 0.0004 * dsin(3 * mprime)
            + 0.0079 * dsin(2 * f)
            - 0.0119 * dsin(m + mprime)
            - 0.0047 * dsin(m - mprime)
            + 0.0003 * dsin(2 * f + m)
            - 0.0004 * dsin(2 * f - m)
            - 0.0006 * dsin(2 * f + mprime)
            + 0.0021 * dsin(2 * f - mprime)
            + 0.0003 * dsin(m + 2 * mprime)
            + 0.0004 * dsin(m - 2 * mprime)
            - 0.0003 * dsin(2 * m + mprime);

        auto dcos_m = dcos(m);
        auto dcos_mprime = dcos(mprime);
        if (phase < 0.5) {
            /* First quarter correction */
            pt += 0.0028 - 0.0004 * dcos_m + 0.0003 * dcos_mprime;
        }
        else {
            pt += -0.0028 + 0.0004 * dcos_m - 0.0003 * dcos_mprime;
        }
    }
    return pt;
}

/*   PHASEHUNT	--  Find time of phases of the moon which surround the
            current date.  Five phases are found, starting and
            ending with the new moons which bound the  current
            lunation.  */

void phaseHunt(double sdate, double phases[5])
{
    auto adate = sdate - 45.0;

    auto ymd = julianYear(adate);
    auto k1 = floor((ymd.year + ((ymd.month - 1) * (1.0 / 12.0)) - 1900) * 12.3685);
    decltype(k1) k2;

    auto nt1 = adate = meanPhase(adate, k1);
    while (true) {
        adate += synmonth;
        k2 = k1 + 1;
        auto nt2 = meanPhase(adate, k2);
        if (nt1 <= sdate && nt2 > sdate) {
            break;
        }
        nt1 = nt2;
        k1 = k2;
    }
    phases[0] = truePhase(k1, 0.0);
    phases[1] = truePhase(k1, 0.25);
    phases[2] = truePhase(k1, 0.5);
    phases[3] = truePhase(k1, 0.75);
    phases[4] = truePhase(k2, 0.0);
}

/*  KEPLER  --	 Solve the equation of Kepler.	*/

static constexpr auto kEpsilon = 1E-6;

double kepler(double m, double ecc)
{
    auto e = m = toRad(m);
    decltype(e) delta;
    do {
        delta = e - ecc * sin(e) - m;
        e -= delta / (1 - ecc * cos(e));
    }
    while (std::abs(delta) > kEpsilon);
    return e;
}

void phaseShort(double pDate, MoonPhaseType &moon)
{
    /* Calculation of the Sun's position */

    /* Date within epoch */
    auto Day = pDate - kEpoch;

    /* Mean anomaly of the Sun */
    auto N = fixAngle((360 / 365.2422) * Day);

    /* Convert from perigee co-ordinates to epoch 1980.0 */
    auto M = fixAngle(N + elonge - elongp);

    /* Solve equation of Kepler */
    auto Ec = kepler(M, eccent);
    Ec = sqrt((1 + eccent) / (1 - eccent)) * tan(Ec / 2);

    /* True anomaly */
    Ec = 2 * toDeg(atan(Ec));

    /* Sun's geocentric ecliptic longitude */
    auto lambdaSun = fixAngle(Ec + elongp);

    /* Calculation of the Moon's position */

    /* Moon's mean longitude */
    auto ml = fixAngle(13.1763966 * Day + mmlong);

    /* Moon's mean anomaly */
    auto MM = fixAngle(ml - 0.1114041 * Day - mmlongp);

    /* Moon's ascending node mean longitude */
    auto MN = fixAngle(mlnode - 0.0529539 * Day);

    /* Evection */
    auto Ev = 1.2739 * dsin(2 * (ml - lambdaSun) - MM);

    /* Annual equation */
    auto Ae = 0.1858 * dsin(M);

    /* Correction term */
    auto A3 = 0.37 * dsin(M);

    /* Corrected anomaly */
    auto MmP = MM + Ev - Ae - A3;

    /* Correction for the equation of the centre */
    auto mEc = 6.2886 * dsin(MmP);

    /* Another correction term */
    auto A4 = 0.214 * dsin(2 * MmP);

    /* Corrected longitude */
    auto lP = ml + Ev + mEc - Ae + A4;

    /* Variation */
    auto V = 0.6583 * dsin(2 * (lP - lambdaSun));

    /* True longitude */
    auto lPP = lP + V;

    /* Corrected longitude of the node */
    auto NP = MN - 0.16 * dsin(M);

    /* Y inclination coordinate */
    auto y = dsin(lPP - NP) * dcos(minc);

    /* X inclination coordinate */
    auto x = dcos(lPP - NP);

    /* Ecliptic longitude */
    auto lambdaMoon = toDeg(atan2(y, x));
    lambdaMoon += NP;

    /* Calculation of the phase of the Moon */

    /* Age of the Moon in degrees */
    auto MoonAge = lPP - lambdaSun;

    /* Illumination of the Moon */
    moon.iPhase = (1 - dcos(MoonAge)) / 2.0;

    /* Phase of the Moon */
    moon.pPhase = fixAngle(MoonAge) / 360.0;

    moon.mAge = synmonth * (fixAngle(MoonAge) / 360.0);
}

#ifndef _MSC_VER
#pragma GCC pop_options
#endif
