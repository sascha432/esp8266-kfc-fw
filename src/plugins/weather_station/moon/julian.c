
#include <math.h>
#include "julian.h"

/*  JDATE  --  Convert internal GMT date and time to  Julian  day  and
           fraction.  */

long jdate(struct tm *t)
{
    long c, m, y;

    y = t->tm_year + 1900;
    m = t->tm_mon + 1;
    if (m > 2) {
        m = m - 3;
    } else {
        m = m + 9;
        y--;
    }
    c = y / 100L; /* Compute century */
    y -= 100L * c;
    return (t->tm_mday + (c * 146097L) / 4 + (y * 1461L) / 4 + (m * 153L + 2) / 5 + 1721119L);
}

/*  JTIME  --  Convert internal GMT  date  and	time  to  astronomical
           Julian	time  (i.e. Julian  date  plus	day  fraction,
           expressed as a double).	*/

double jtime(struct tm *t)
{
    return (jdate(t) - 0.5) + (t->tm_sec + 60 * (t->tm_min + 60 * t->tm_hour)) / 86400.0;
}

/*  JYEAR  --  Convert	Julian	date  to  year,  month, day, which are
           returned via integer pointers to integers.  */

void jyear(double td, int *yy, int *mm, int *dd)
{
    double j, d, y, m;

    td += 0.5; /* Astronomical to civil */
    j = floor(td);
    j = j - 1721119.0;
    y = floor(((4 * j) - 1) / 146097.0);
    j = (j * 4.0) - (1.0 + (146097.0 * y));
    d = floor(j / 4.0);
    j = floor(((4.0 * d) + 3.0) / 1461.0);
    d = ((4.0 * d) + 3.0) - (1461.0 * j);
    d = floor((d + 4.0) / 4.0);
    m = floor(((5.0 * d) - 3) / 153.0);
    d = (5.0 * d) - (3.0 + (153.0 * m));
    d = floor((d + 5.0) / 5.0);
    y = (100.0 * y) + j;
    if (m < 10.0) {
        m = m + 3;
    }
    else {
        m = m - 9;
        y = y + 1;
    }
    *yy = y;
    *mm = m;
    *dd = d;
}

/*  JHMS  --  Convert Julian time to hour, minutes, and seconds.  */

void jhms(double j, int *h, int *m, int *s)
{
    long ij;

    j += 0.5; /* Astronomical to civil */
    ij = (j - floor(j)) * 86400.0;
    *h = ij / 3600L;
    *m = (ij / 60L) % 60L;
    *s = ij % 60L;
}
