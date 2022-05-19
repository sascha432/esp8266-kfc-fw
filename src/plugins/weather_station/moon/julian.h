/**
 * Modified by: sascha_lammers@gmx.de
 */

#pragma once

#include <time.h>

// julianDate -- Convert internal GMT date and time to Julian day and fraction.
long julianDate(struct tm *t);

// julianTime -- Convert internal GMT date and time to astronomical Julian time
// (i.e. Julian date plus day fraction, expressed as a double).
double julianTime(struct tm *t);

struct julianYearType {
    uint16_t year;
    uint8_t month;

    julianYearType(double y, double m) : year(y), month(m) {}
};

// julianYear -- Convert Julian date to year and month
julianYearType julianYear(double td);
