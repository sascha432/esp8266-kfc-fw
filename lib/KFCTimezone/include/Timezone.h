/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <time.h>

#define IS_TIME_VALID(time) (time > (30 * 86400))

class Timezone {
public:
	Timezone();

    void invalidate();
	bool isValid() const;

	void setTimezone(time_t now, const char *zoneName);
    void setTimezone(time_t now, const String zoneName);
	const String &getTimezone() const;

	void setAbbreviation(const char *abbreviation);
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
	const int32_t INVALID = -1;

    int32_t _timezoneOffset;
    bool _dst;
    String _zoneName;
    String _abbreviation;
};

Timezone &get_default_timezone();
tm *timezone_localtime(const time_t *timer);
size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm);
size_t timezone_strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm);
size_t timezone_strftime(char *buf, size_t size, PGM_P format, const struct tm *tm);
