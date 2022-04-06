/**
  Author: sascha_lammers@gmx.de
*/

#define __XSI_VISIBLE

#include "misc_time.h"
#include <Arduino_compat.h>
#include <PrintString.h>
#include <debug_helper.h>
#include <stdlib.h>
#include <time.h>

#define DEBUG_MISC_TIME 0

#if DEBUG_MISC_TIME
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

static time_t ntpLastKnownTime = TIME_T_MIN;

void safeSetTZ(const String &timezone)
{
    safeSetTZ(timezone.c_str());
}

#if ESP8266 && (TZ_ENVIRONMENT_MAX_SIZE >= 8)

// optimized version that does not suffer from memory leaks using setenv quite often

static constexpr size_t kMaxTZData = TZ_ENVIRONMENT_MAX_SIZE;

extern "C" char **environ;
static char *tzPtr;
static const char tzEnv[] PROGMEM = { "TZ" };
static const char tzEnvEquals[] PROGMEM = { "TZ=" };

static char **_getTZEnvironmentEntry()
{
    char **ptr = environ;
    while (*ptr) {
        if (strncmp_P(*ptr, tzEnvEquals, sizeof(tzEnvEquals) - 1) == 0) {
            __LDBG_printf("env %p:%s buf %p:%s", ptr ? *ptr : nullptr, __S(ptr ? *ptr : nullptr), tzPtr, __S(tzPtr));
            return ptr;
        }
        ptr++;
    }
    return nullptr;
}

void safeSetTZ(const __FlashStringHelper *timezone)
{
    // find if the entry exists and matches with our buffer
    auto entry = _getTZEnvironmentEntry();
    __LDBG_printf("setenv %p ptr %p data %s", entry, entry ? *entry : nullptr, (entry && tzPtr == *entry) ? printable_string(*entry, kMaxTZData, kMaxTZData).c_str() : PSTR("N/A"));
    if (!entry || (tzPtr != *entry)) {
        // if there is no entry, create a dummy for 64 characters
        auto bufferPtr = std::unique_ptr<char>(new char[kMaxTZData + 1]);
        auto buf = bufferPtr.get();

        std::fill_n(buf, kMaxTZData, 'A');
        buf[kMaxTZData] = 0;

        if (setenv(CStrP(tzEnv), buf, 1) == 0) {
            entry = _getTZEnvironmentEntry();
            if (!entry) {
                __DBG_printf("could not find TZ after setenv %p ptr %p", entry, entry ? *entry : nullptr);
                tzPtr = nullptr;
                return;
            }
            tzPtr = *entry;
        } else {
            // save nullptr in case of  failure
            tzPtr = nullptr;
        }
    }

    #if DEBUG_MISC_TIME
        entry = _getTZEnvironmentEntry();
        if (!entry || (tzPtr != *entry)) {
            __DBG_panic("could not find TZ env entry %p ptr %p", entry, entry ? *entry : nullptr);
            return;
        }
    #endif

    // update the content of the pointer
    snprintf_P(tzPtr, kMaxTZData + 3, PSTR("TZ=%s"), timezone);

    tzset();
}

#else

void safeSetTZ(const __FlashStringHelper *timezone)
{
    setenv(CStrP(F("TV")), CStrP(timezone), 1);
    tzset();
}

#endif

time_t getLastKnownTimeOfDay()
{
    return ntpLastKnownTime;
}

void setLastKnownTimeOfDay(time_t time)
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
    SPGM(year), // 0
    SPGM(month), // 1
    SPGM(week), // 2
    SPGM(day), // 3
    SPGM(hour), // 4
    SPGM(minute), // 5
    SPGM(second), // 6
    SPGM(millisecond), // 7
    SPGM(microsecond), // 8
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
    switch (items.size()) {
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
    for (const auto &str : items) {
        len += str.length();
    }
    auto end = std::prev(items.end());
    auto iterator = items.begin();
    String output = std::move(items[0]);
    output.reserve(len);
    for (++iterator; iterator != end; ++iterator) {
        output += sep;
        output += *iterator;
    }
    output += lastSep;
    output += *iterator;
    return output;
}
