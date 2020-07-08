/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <time.h>
#if defined(ESP8266)
#include <sntp.h>
#include <sntp-lwip2.h>
#elif defined(ESP32)
#include <lwip/apps/sntp.h>
#endif
#include "Timezone.h"

Timezone default_timezone;


#if ESP8266

// extern char *_tzname[2];
// extern int daylight;

char *Timezone::_GMT = _tzname[0];

#else

const char Timezone::_GMT[] PROGMEM = { "GMT" };

#endif

Timezone &get_default_timezone()
{
    return default_timezone;
}

tm *timezone_localtime(const time_t *timer)
{
#if ESP8266
    return localtime(timer);
#else
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
#endif
}

size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm)
{
#ifdef _WIN32
    auto fmt = format;
#else
    char fmt[strlen_P(format) + 1];
    strcpy_P(fmt, format);
#endif
    return strftime(buf, size, fmt, tm);
}

size_t timezone_strftime(char *buf, size_t size, const char *format, const struct tm *tm)
{
    return strftime(buf, size, format, tm);
}

size_t timezone_strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm)
{
#if ESP8266
    return strftime_P(buf, size, format, tm);
#else
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
		fmt.replace(_z, emptyString);
        _z.toUpperCase();
        fmt.replace(_z, emptyString);
        return strftime(buf, size, fmt.c_str(), tm);
    }
#endif
}


Timezone::Timezone()
{
	invalidate();
}

void Timezone::invalidate()
{
    _timezoneOffset = Timezone::INVALID;
    _dst = false;
    _abbreviation = _GMT;
    _zoneName = String();
}

void Timezone::setTimezone(time_t now, const char *zoneName)
{
	_zoneName = zoneName;
}

void Timezone::setTimezone(time_t now, const String zoneName)
{
    _zoneName = zoneName;
}

const String &Timezone::getTimezone() const
{
    return _zoneName;
}

void Timezone::setAbbreviation(const String abbreviation)
{
    _abbreviation = abbreviation;
}

const String &Timezone::getAbbreviation() const
{
    return _abbreviation;
}

int32_t Timezone::getOffset() const
{
	return _timezoneOffset;
}

bool Timezone::isValid() const
{
	return _timezoneOffset != Timezone::INVALID;
}

void Timezone::setOffset(int32_t offset)
{
	_timezoneOffset = offset;
}

void Timezone::setDst(bool dst)
{
	_dst = dst;
}

bool Timezone::isDst() const
{
	return _dst;
}

void Timezone::load()
{
#ifndef _WIN32
    extern void timezone_config_load(int32_t &_timezoneOffset, bool &_dst, String &_zoneName, String &_abbreviation);
    timezone_config_load(_timezoneOffset, _dst, _zoneName, _abbreviation);
#endif
}

void Timezone::save()
{
#ifndef _WIN32
    extern void timezone_config_save(int32_t _timezoneOffset, bool _dst, const String &_zoneName, const String &_abbreviation);
    timezone_config_save(_timezoneOffset, _dst, _zoneName, _abbreviation);
#endif
}
