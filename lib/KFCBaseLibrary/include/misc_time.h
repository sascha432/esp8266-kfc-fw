/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <time.h>

// use TIME_T_FMT for sprintf and time_t
#if defined(_MSC_VER)

#ifdef _WIN32

#define TIME_T_FMT "%ld"
#define TIME_T_MIN 946684800L       // Sat Jan 01 2000 00:00:00 GMT+0000
#define TIME_T_MAX 0x7FFFFFFFL      // Tue Jan 19 2038 03:14:07 GMT+0000

#else

#erro TODO

#endif

#elif __GNUG__ && __GNUC__ < 5

#define TIME_T_FMT "%ld"
#define TIME_T_MIN 946684800L       // Sat Jan 01 2000 00:00:00 GMT+0000
#define TIME_T_MAX 0x7FFFFFFFL      // Tue Jan 19 2038 03:14:07 GMT+0000

#else

#define TIME_T_FMT "%lld"
#define TIME_T_MIN 946684800LL      // Sat Jan 01 2000 00:00:00 GMT+0000
#define TIME_T_MAX 0xFFFFFFFFLL     // Sun Feb 07 2106 06:28:15 GMT+0000

#endif

time_t getLastKnownTimeOfDay();
void setLastKnownTimeOfDay(time_t time);

inline static bool isTimeValid(time_t time) {
    return time >= getLastKnownTimeOfDay();
}

inline static bool isTimeValid() {
    return isTimeValid(time(nullptr));
}

size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm);

// seconds since boot
uint32_t getSystemUptime();

// milliseconds
uint64_t getSystemUptimeMillis();
