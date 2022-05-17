/**
 * Modified by: sascha_lammers@gmx.de
 */

#pragma once

#include <time.h>

// JDATE -- Convert internal GMT date and time to Julian day and fraction.
long jdate(struct tm *t);

// JTIME -- Convert internal GMT date and time to astronomical Julian time
// (i.e. Julian date plus day fraction, expressed as a double).
double jtime(struct tm *t);

// JYEAR -- Convert Julian date to year, month, day, which are returned via integer pointers to integers.
void jyear(double td, int *yy, int *mm, int *dd);

// JHMS -- Convert Julian time to hour, minutes, and seconds.
void jhms(double j, int *h, int *m, int *s);
