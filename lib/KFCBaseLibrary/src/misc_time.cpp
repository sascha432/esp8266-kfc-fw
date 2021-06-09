/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include "misc_time.h"

static time_t ntpLastKnownTime = TIME_T_MIN;

time_t getLastKnownTime()
{
    return ntpLastKnownTime;
}

void setLastKnownTime(time_t time)
{
    ntpLastKnownTime = time;
}

//
// example
//  1 day 05:23:42
//
String formatTime(unsigned long seconds, bool printDaysIfZero)
{
    PrintString out;
    auto days = (uint32_t)(seconds / 86400U);
    if (printDaysIfZero || days) {
        out.printf_P(PSTR("%u day"), days);
        if (days != 1) {
            out.print('s');
        }
        out.print(' ');
    }
    out.printf_P(PSTR("%02uh:%02um:%02us"), (unsigned)((seconds / 3600) % 24), (unsigned)((seconds / 60) % 60), (unsigned)(seconds % 60));
    return out;
}

const char *formatTimeNames_long[] PROGMEM = {
    SPGM(year),                     // 0
    SPGM(month),                    // 1
    SPGM(week),                     // 2
    SPGM(day),                      // 3
    SPGM(hour),                     // 4
    SPGM(minute),                   // 5
    SPGM(second),                   // 6
    SPGM(millisecond),              // 7
    SPGM(microsecond),              // 8
};

const char *formatTimeNames_short[] PROGMEM = {
    SPGM(yr),
    SPGM(mth),
    SPGM(wk),
    SPGM(day),
    SPGM(hr),
    SPGM(min),
    SPGM(sec),
    SPGM(ms),
    SPGM(UTF8_microseconds),
};

String __formatTime(PGM_P names[], bool isShort, const String &sep, const String &_lastSep, bool displayZero, int seconds, int minutes, int hours, int days, int weeks, int months, int years, int milliseconds, int microseconds)
{
    StringVector items;
    auto &lastSep = _lastSep.length() ? _lastSep : sep;
    auto minValue = displayZero ? 0 : 1;

    if (years >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), years, names[0], years == 1 ? emptyString.c_str() : PSTR("s")));
    }
    if (months >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), months, names[1], months == 1 ? emptyString.c_str() : PSTR("s")));
    }
    if (weeks >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), weeks, names[2], weeks == 1 ? emptyString.c_str() : PSTR("s")));
    }
    if (days >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), days, names[3], days == 1 ? emptyString.c_str() : PSTR("s")));
    }
    if (hours >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), hours, names[4], hours == 1 ? emptyString.c_str() : PSTR("s")));
    }
    if (minutes >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), minutes, names[5], (minutes == 1) || isShort ? emptyString.c_str() : PSTR("s")));
    }
    if (seconds >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), seconds, names[6], (seconds == 1) || isShort ? emptyString.c_str() : PSTR("s")));
    }
    if (milliseconds >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), milliseconds, names[7], (milliseconds == 1) || isShort ? emptyString.c_str() : PSTR("s")));
    }
    if (microseconds >= minValue) {
        items.emplace_back(PrintString(F("%u %s%s"), microseconds, names[8], (microseconds == 1) || isShort ? emptyString.c_str() : PSTR("s")));
    }
    switch(items.size()) {
        case 0:
            return F("N/A");
        case 1:
            return items[0];
        case 2:
            return items[0] + lastSep + items[1];
        default:
        break;
    }
    size_t len = (sep.length() * (items.size() - 1)) + lastSep.length();
    for(const auto &str: items) {
        len += str.length();
    }
    auto end = std::prev(items.end());
    auto iterator = items.begin();
    String output = std::move(items[0]);
    output.reserve(len);
    for(++iterator; iterator != end; ++iterator) {
        output += sep;
        output += *iterator;
    }
    output += lastSep;
    output += *iterator;
    return output;
}
