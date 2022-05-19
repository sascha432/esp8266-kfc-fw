/**
 * Modified by: sascha_lammers@gmx.de
 */

#include "julian.h"
#include <math.h>

#ifndef _MSC_VER
#pragma GCC push_options
#pragma GCC optimize ("Os")
#endif

// JDATE -- Convert internal GMT date and time to Julian day and fraction.

long julianDate(struct tm *t)
{
    long c, m, y;

    y = t->tm_year + 1900;
    m = t->tm_mon + 1;
    if (m > 2) {
        m = m - 3;
    }
    else {
        m = m + 9;
        y--;
    }
    c = y / 100L; /* Compute century */
    y -= 100L * c;
    return (t->tm_mday + (c * 146097L) / 4 + (y * 1461L) / 4 + (m * 153L + 2) / 5 + 1721119L);
}

// JTIME -- Convert internal GMT date and time to astronomical Julian time
// (i.e. Julian date plus day fraction, expressed as a double).

double julianTime(struct tm *t)
{
    return (julianDate(t) - 0.5) + (t->tm_sec + 60 * (t->tm_min + 60 * t->tm_hour)) / 86400.0;
}

// JYEAR -- Convert Julian date to year and month

julianYearType julianYear(double td)
{
    td += 0.5; /* Astronomical to civil */
    auto j = floor(td);
    j = j - 1721119.0;
    auto y = floor(((4 * j) - 1) / 146097.0);
    j = (j * 4.0) - (1.0 + (146097.0 * y));
    auto d = floor(j / 4.0);
    j = floor(((4.0 * d) + 3.0) / 1461.0);
    d = ((4.0 * d) + 3.0) - (1461.0 * j);
    d = floor((d + 4.0) / 4.0);
    auto m = floor(((5.0 * d) - 3) / 153.0);
    y = (100.0 * y) + j;
    if (m < 10.0) {
        m = m + 3;
    }
    else {
        m = m - 9;
        y = y + 1;
    }
    return julianYearType(y, m);
}

#ifndef _MSC_VER
#pragma GCC pop_options
#endif
