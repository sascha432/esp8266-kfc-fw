/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <time.h>
#include <limits.h>

#if 1

typedef char end_of_time_t_2038[(time_t)(1UL<<31)==INT32_MIN ? 1 : -1];
#define IS_TIME_VALID(time) (((time_t)time < 0) || time > 946684800L)

#else

typedef char end_of_time_t_2106[(time_t)(1UL<<31)==(INT32_MAX+1UL) ? 1 : -1];
#define IS_TIME_VALID(time) (time > 946684800UL)

#endif

class Timezone {
public:
	Timezone();

    void invalidate();
	bool isValid() const;

	void setTimezone(time_t now, const char *zoneName);
    void setTimezone(time_t now, const String zoneName);
	const String &getTimezone() const;

    void setAbbreviation(const String abbreviation);
	const String &getAbbreviation() const;

	int32_t getOffset() const;
	void setOffset(int32_t offset);

	// daylight savings time
	void setDst(bool dst);
	bool isDst() const;

	void load();
	void save();

private:
	static constexpr int32_t INVALID = -1;

    int32_t _timezoneOffset;
    bool _dst;
    String _zoneName;
    String _abbreviation;

public:
#if ESP8266
	static char *_GMT;
#else
	static const char _GMT[] PROGMEM;
#endif
};

Timezone &get_default_timezone();
tm *timezone_localtime(const time_t *timer);
size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm);
size_t timezone_strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm);
size_t timezone_strftime(char *buf, size_t size, const char *format, const struct tm *tm);
