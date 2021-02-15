/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <time.h>
#include "strtotime.h"

namespace TimeParser {

    enum class ResultType : int8_t {
        ERROR = -1,
        NONE = 0,
        UNIXTIME,
        BOOTTIME,
        RECURRING
    };

    class Result {
    public:
        Result() : _result(ResultType::NONE) {}

    private:
        ResultType _result;
        union {
            uint64_t _timestamp;
            struct tm _recurring;
        };
    };

    class Parser {
    public:
        Parser() :
            _timeStr(nullptr)
        {}

        void setTimeStr(const char *str) {
            _timeStr = str;
        }
        void setTimeStr(const String &str) {
            setTimeStr(str.c_str());
        }
        void setTimeStr(const __FlashStringHelper *str) {
            setTimeStr(reinterpret_cast<PGM_P>(str));
        }

        void parse();

    private:
        const char *_timeStr;

    };

}

/*
static const timelib_tz_lookup_table timelib_timezone_utc[] = {
	{ "utc", 0, 0, "UTC" },
};

static timelib_relunit const timelib_relunit_lookup[] = {
	{ "ms",           TIMELIB_MICROSEC, 1000 },
	{ "msec",         TIMELIB_MICROSEC, 1000 },
	{ "msecs",        TIMELIB_MICROSEC, 1000 },
	{ "millisecond",  TIMELIB_MICROSEC, 1000 },
	{ "milliseconds", TIMELIB_MICROSEC, 1000 },
	{ "µs",           TIMELIB_MICROSEC,    1 },
	{ "usec",         TIMELIB_MICROSEC,    1 },
	{ "usecs",        TIMELIB_MICROSEC,    1 },
	{ "µsec",         TIMELIB_MICROSEC,    1 },
	{ "µsecs",        TIMELIB_MICROSEC,    1 },
	{ "microsecond",  TIMELIB_MICROSEC,    1 },
	{ "microseconds", TIMELIB_MICROSEC,    1 },
	{ "sec",         TIMELIB_SECOND,  1 },
	{ "secs",        TIMELIB_SECOND,  1 },
	{ "second",      TIMELIB_SECOND,  1 },
	{ "seconds",     TIMELIB_SECOND,  1 },
	{ "min",         TIMELIB_MINUTE,  1 },
	{ "mins",        TIMELIB_MINUTE,  1 },
	{ "minute",      TIMELIB_MINUTE,  1 },
	{ "minutes",     TIMELIB_MINUTE,  1 },
	{ "hour",        TIMELIB_HOUR,    1 },
	{ "hours",       TIMELIB_HOUR,    1 },
	{ "day",         TIMELIB_DAY,     1 },
	{ "days",        TIMELIB_DAY,     1 },
	{ "week",        TIMELIB_DAY,     7 },
	{ "weeks",       TIMELIB_DAY,     7 },
	{ "fortnight",   TIMELIB_DAY,    14 },
	{ "fortnights",  TIMELIB_DAY,    14 },
	{ "forthnight",  TIMELIB_DAY,    14 },
	{ "forthnights", TIMELIB_DAY,    14 },
	{ "month",       TIMELIB_MONTH,   1 },
	{ "months",      TIMELIB_MONTH,   1 },
	{ "year",        TIMELIB_YEAR,    1 },
	{ "years",       TIMELIB_YEAR,    1 },

	{ "monday",      TIMELIB_WEEKDAY, 1 },
	{ "mon",         TIMELIB_WEEKDAY, 1 },
	{ "tuesday",     TIMELIB_WEEKDAY, 2 },
	{ "tue",         TIMELIB_WEEKDAY, 2 },
	{ "wednesday",   TIMELIB_WEEKDAY, 3 },
	{ "wed",         TIMELIB_WEEKDAY, 3 },
	{ "thursday",    TIMELIB_WEEKDAY, 4 },
	{ "thu",         TIMELIB_WEEKDAY, 4 },
	{ "friday",      TIMELIB_WEEKDAY, 5 },
	{ "fri",         TIMELIB_WEEKDAY, 5 },
	{ "saturday",    TIMELIB_WEEKDAY, 6 },
	{ "sat",         TIMELIB_WEEKDAY, 6 },
	{ "sunday",      TIMELIB_WEEKDAY, 0 },
	{ "sun",         TIMELIB_WEEKDAY, 0 },

	{ "weekday",     TIMELIB_SPECIAL, TIMELIB_SPECIAL_WEEKDAY },
	{ "weekdays",    TIMELIB_SPECIAL, TIMELIB_SPECIAL_WEEKDAY },
	{ NULL,          0,          0 }
};


static timelib_lookup_table const timelib_reltext_lookup[] = {
	{ "first",    0,  1 },
	{ "next",     0,  1 },
	{ "second",   0,  2 },
	{ "third",    0,  3 },
	{ "fourth",   0,  4 },
	{ "fifth",    0,  5 },
	{ "sixth",    0,  6 },
	{ "seventh",  0,  7 },
	{ "eight",    0,  8 },
	{ "eighth",   0,  8 },
	{ "ninth",    0,  9 },
	{ "tenth",    0, 10 },
	{ "eleventh", 0, 11 },
	{ "twelfth",  0, 12 },
	{ "last",     0, -1 },
	{ "previous", 0, -1 },
	{ "this",     1,  0 },
	{ NULL,       1,  0 }
};


static timelib_lookup_table const timelib_month_lookup[] = {
	{ "jan",  0,  1 },
	{ "feb",  0,  2 },
	{ "mar",  0,  3 },
	{ "apr",  0,  4 },
	{ "may",  0,  5 },
	{ "jun",  0,  6 },
	{ "jul",  0,  7 },
	{ "aug",  0,  8 },
	{ "sep",  0,  9 },
	{ "sept", 0,  9 },
	{ "oct",  0, 10 },
	{ "nov",  0, 11 },
	{ "dec",  0, 12 },
	{ "i",    0,  1 },
	{ "ii",   0,  2 },
	{ "iii",  0,  3 },
	{ "iv",   0,  4 },
	{ "v",    0,  5 },
	{ "vi",   0,  6 },
	{ "vii",  0,  7 },
	{ "viii", 0,  8 },
	{ "ix",   0,  9 },
	{ "x",    0, 10 },
	{ "xi",   0, 11 },
	{ "xii",  0, 12 },

	{ "january",   0,  1 },
	{ "february",  0,  2 },
	{ "march",     0,  3 },
	{ "april",     0,  4 },
	{ "may",       0,  5 },
	{ "june",      0,  6 },
	{ "july",      0,  7 },
	{ "august",    0,  8 },
	{ "september", 0,  9 },
	{ "october",   0, 10 },
	{ "november",  0, 11 },
	{ "december",  0, 12 },
	{  NULL,       0,  0 }
};
*/
