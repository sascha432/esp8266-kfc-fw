/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "Timezone.h"

Timezone default_timezone;

Timezone &get_default_timezone() {
    return default_timezone;
}

tm *timezone_localtime(const time_t *timer) {
    struct tm *_tm;
    time_t now;
    if (!timer) {
        time(&now);
    } else {
        now = *timer;
    }
    if (default_timezone.isValid()) {
        now += default_timezone.getOffset();
        _tm = gmtime(&now);
        _tm->tm_isdst = default_timezone.isDst() ? 1 : 0;
    } else {
        _tm = localtime(&now);
    }
    return _tm;
}

size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm) {
    String fmt = FPSTR(format);
    return strftime(buf, size, fmt.c_str(), tm);
}

size_t timezone_strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm) {
    return timezone_strftime(buf, size, format, tm);
}

size_t timezone_strftime(char *buf, size_t size, PGM_P format, const struct tm *tm) {
	String _z = F("%z");
    String fmt = FPSTR(format);
    if (default_timezone.isValid()) {
        if (fmt.indexOf(_z) != -1) {
            int32_t ofs = default_timezone.getOffset();
            char buf[8];
            snprintf_P(buf, sizeof(buf), PSTR("%c%02u:%02u"), ofs < 0 ? '-' : '+', (int)(std::abs(ofs) / 3600) % 24, (int)(std::abs(ofs) % 60));
            fmt.replace(_z, buf);
        }
		_z.toUpperCase();
        fmt.replace(_z, default_timezone.getAbbreviation());
        return strftime(buf, size, fmt.c_str(), tm);
    } else {
		fmt.replace(_z, _sharedEmptyString);
        _z.toUpperCase();
        fmt.replace(_z, _sharedEmptyString);
        return strftime(buf, size, fmt.c_str(), tm);
    }
}


Timezone::Timezone() {
	invalidate();
}

void Timezone::invalidate() {
    _timezoneOffset = Timezone::INVALID;
    _dst = false;
    _abbreviation = String();
    _zoneName = String();
}

void Timezone::setTimezone(time_t now, const char *zoneName) {
	_zoneName = zoneName;
}

void Timezone::setTimezone(time_t now, const String zoneName) {
    _zoneName = zoneName;
}

const String &Timezone::getTimezone() {
    return _zoneName;
}

void Timezone::setAbbreviation(const char *abbreviation) {
	_abbreviation = abbreviation;
}

void Timezone::setAbbreviation(const String abbreviation) {
    _abbreviation = abbreviation;
}

const String &Timezone::getAbbreviation() {
    return _abbreviation;
}

int32_t Timezone::getOffset() {
	return _timezoneOffset;
}

bool Timezone::isValid() {
	return _timezoneOffset != Timezone::INVALID;
}

void Timezone::setOffset(int32_t offset) {
	_timezoneOffset = offset;
}

void Timezone::setDst(bool dst) {
	_dst = dst;
}

bool Timezone::isDst() {
	return _dst;
}
