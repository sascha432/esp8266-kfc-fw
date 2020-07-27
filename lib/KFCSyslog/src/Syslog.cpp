/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogFilter.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(kern, "kern");
PROGMEM_STRING_DEF(user, "user");
PROGMEM_STRING_DEF(mail, "mail");
PROGMEM_STRING_DEF(daemon, "daemon");
PROGMEM_STRING_DEF(secure, "secure");
PROGMEM_STRING_DEF(syslog, "syslog");
PROGMEM_STRING_DEF(ntp, "ntp");
PROGMEM_STRING_DEF(local0, "local0");
PROGMEM_STRING_DEF(local1, "local1");
PROGMEM_STRING_DEF(local2, "local2");
PROGMEM_STRING_DEF(local3, "local3");
PROGMEM_STRING_DEF(local4, "local4");
PROGMEM_STRING_DEF(local5, "local5");
PROGMEM_STRING_DEF(local6, "local6");
PROGMEM_STRING_DEF(local7, "local7");

PROGMEM_STRING_DEF(emerg, "emerg");
PROGMEM_STRING_DEF(emergency, "emergency");
PROGMEM_STRING_DEF(alert, "alert");
PROGMEM_STRING_DEF(crit, "crit");
PROGMEM_STRING_DEF(critical, "critical");
PROGMEM_STRING_DEF(err, "err");
PROGMEM_STRING_DEF(error, "error");
PROGMEM_STRING_DEF(warn, "warn");
PROGMEM_STRING_DEF(warning, "warning");
PROGMEM_STRING_DEF(notice, "notice");
PROGMEM_STRING_DEF(info, "info");
PROGMEM_STRING_DEF(debug, "debug");

const SyslogFilterItemPair syslogFilterFacilityItems[] PROGMEM = {
    { SPGM(kern), SYSLOG_FACILITY_KERN },
    { SPGM(user), SYSLOG_FACILITY_USER },
    { SPGM(mail), SYSLOG_FACILITY_MAIL },
    { SPGM(daemon), SYSLOG_FACILITY_DAEMON },
    { SPGM(secure), SYSLOG_FACILITY_SECURE },
    { SPGM(syslog), SYSLOG_FACILITY_SYSLOG },
    { SPGM(ntp), SYSLOG_FACILITY_NTP },
    { SPGM(local0), SYSLOG_FACILITY_LOCAL0 },
    { SPGM(local1), SYSLOG_FACILITY_LOCAL1 },
    { SPGM(local2), SYSLOG_FACILITY_LOCAL2 },
    { SPGM(local3), SYSLOG_FACILITY_LOCAL3 },
    { SPGM(local4), SYSLOG_FACILITY_LOCAL4 },
    { SPGM(local5), SYSLOG_FACILITY_LOCAL5 },
    { SPGM(local6), SYSLOG_FACILITY_LOCAL6 },
    { SPGM(local7), SYSLOG_FACILITY_LOCAL7 },
    { nullptr, 0xff },
};

const SyslogFilterItemPair syslogFilterSeverityItems[] PROGMEM = {
    { SPGM(emerg), SYSLOG_EMERG },
	{ SPGM(emergency), SYSLOG_EMERG },
	{ SPGM(alert), SYSLOG_ALERT },
	{ SPGM(crit), SYSLOG_CRIT },
	{ SPGM(critical), SYSLOG_CRIT },
	{ SPGM(err), SYSLOG_ERR },
	{ SPGM(error), SYSLOG_ERR },
	{ SPGM(warn), SYSLOG_WARN },
	{ SPGM(warning), SYSLOG_WARN },
	{ SPGM(notice), SYSLOG_NOTICE },
	{ SPGM(info), SYSLOG_INFO },
	{ SPGM(debug), SYSLOG_DEBUG },
	{ nullptr, 0xff },
};

SyslogFilterItem::SyslogFilterItem(const String &facility, const String &severity) : SyslogFilterItem(Syslog::facilityToInt(facility), Syslog::severityToInt(severity)) {
}


SyslogFileFilterItem::SyslogFileFilterItem(const String &filter, Syslog *syslog) : _syslog(syslog)
{
	_parseFilter(filter, _filter);
}

bool SyslogFileFilterItem::isStop() const
{
	return _syslog == SyslogFilter::kFilterStop;
}

bool SyslogFileFilterItem::isMatch(SyslogFacility _facility, SyslogSeverity _severity)
{
	for (auto &item : _filter) {
		if (item.isMatch(_facility, _severity)) {
			return true;
		}
	}
	return false;
}

void SyslogFileFilterItem::_parseFilter(const String &filter, SyslogFilterItemVector &filters)
{
    int startPos = 0;
    do {
        String severity, facility;
        int endPos = filter.indexOf(',', startPos);
        facility = endPos == -1 ? filter.substring(startPos) : filter.substring(startPos, endPos);
        int splitPos = facility.indexOf('.');
        if (splitPos != -1) {
            severity = facility.substring(splitPos + 1);
            facility.remove(splitPos);
        }
        filters.push_back(SyslogFilterItem(facility, severity));
        startPos = endPos + 1;
    } while (startPos);
}


Syslog::Syslog(SyslogParameter &parameter) : _parameter(parameter)
{
	_debug_printf_P(PSTR("Syslog::Syslog(%s,%s,%s)\n"), parameter.getHostname().c_str(), parameter.getAppName().c_str(), parameter.getProcessId().c_str());
}

void Syslog::transmit(const String &message, Callback_t callback)
{
	_debug_printf_P(PSTR("Syslog::transmit(%s)\n"), message.c_str());
	callback(true);
}

void Syslog::_addTimestamp(String& buffer, PGM_P format)
{
	auto now = time(nullptr);
	auto tm = localtime(&now);
	if (tm && IS_TIME_VALID(now)) {
		char buf[40];
		strftime_P(buf, sizeof(buf), format, tm);
		buffer += buf;
#if SEND_NILVALUE_IF_INVALID_TIMESTAMP == 0
		buffer += ' ';
#endif
	}
#if SEND_NILVALUE_IF_INVALID_TIMESTAMP
	else {
		buffer += '-';
	}
	buffer += ' ';
#endif
}

void Syslog::_addParameter(String& buffer, const String& value) {
	if (value.length() == 0) {
		buffer += '-';
	} else {
		buffer += value;
	}
	buffer += ' ';
}

/*

The syslog message has the following ABNF [RFC5234] definition:

SYSLOG-MSG      = HEADER SP STRUCTURED-DATA [SP MSG]

HEADER          = PRI VERSION SP TIMESTAMP SP HOSTNAME
SP APP-NAME SP PROCID SP MSGID
PRI             = "<" PRIVAL ">"
PRIVAL          = 1*3DIGIT ; range 0 .. 191
VERSION         = NONZERO-DIGIT 0*2DIGIT
HOSTNAME        = NILVALUE / 1*255PRINTUSASCII

APP-NAME        = NILVALUE / 1*48PRINTUSASCII
PROCID          = NILVALUE / 1*128PRINTUSASCII
MSGID           = NILVALUE / 1*32PRINTUSASCII

TIMESTAMP       = NILVALUE / FULL-DATE "T" FULL-TIME
FULL-DATE       = DATE-FULLYEAR "-" DATE-MONTH "-" DATE-MDAY
DATE-FULLYEAR   = 4DIGIT
DATE-MONTH      = 2DIGIT  ; 01-12
DATE-MDAY       = 2DIGIT  ; 01-28, 01-29, 01-30, 01-31 based on
; month/year
FULL-TIME       = PARTIAL-TIME TIME-OFFSET
PARTIAL-TIME    = TIME-HOUR ":" TIME-MINUTE ":" TIME-SECOND
[TIME-SECFRAC]
TIME-HOUR       = 2DIGIT  ; 00-23
TIME-MINUTE     = 2DIGIT  ; 00-59
TIME-SECOND     = 2DIGIT  ; 00-59
TIME-SECFRAC    = "." 1*6DIGIT
TIME-OFFSET     = "Z" / TIME-NUMOFFSET
TIME-NUMOFFSET  = ("+" / "-") TIME-HOUR ":" TIME-MINUTE

STRUCTURED-DATA = NILVALUE / 1*SD-ELEMENT
SD-ELEMENT      = "[" SD-ID *(SP SD-PARAM) "]"
SD-PARAM        = PARAM-NAME "=" %d34 PARAM-VALUE %d34
SD-ID           = SD-NAME
PARAM-NAME      = SD-NAME
PARAM-VALUE     = UTF-8-STRING ; characters '"', '\' and
; ']' MUST be escaped.
SD-NAME         = 1*32PRINTUSASCII
; except '=', SP, ']', %d34 (")

MSG             = MSG-ANY / MSG-UTF8
MSG-ANY         = *OCTET ; not starting with BOM
MSG-UTF8        = BOM UTF-8-STRING
BOM             = %xEF.BB.BF

UTF-8-STRING    = *OCTET ; UTF-8 string as specified
; in RFC 3629

OCTET           = %d00-255
SP              = %d32
PRINTUSASCII    = %d33-126
NONZERO-DIGIT   = %d49-57
DIGIT           = %d48 / NONZERO-DIGIT
NILVALUE        = "-"
*/

void Syslog::addHeader(String& buffer) {
#if SYSLOG_USE_RFC5424

	buffer += '<';
	buffer += String(_parameter.getFacility() << 3 | _parameter.getSeverity());
	buffer += '>';
	_addParameter(buffer, F(SYSLOG_VERSION));
	_addTimestamp(buffer);
	_addParameter(buffer, _parameter.getHostname());
	_addParameter(buffer, _parameter.getAppName());
	_addParameter(buffer, _parameter.getProcessId());
	buffer += F("- - ");  // NIL values for message id & structured data

#else

	// format used by "rsyslog/stable,now 8.24.0-1 armhf"
	// <PRIVAL> SP TIMESTAMP SP HOSTNAME SP APPNAME ["[" PROCESSID "]"] ":" SP MESSAGE
	// the process id part is optional

	buffer += '<';
	buffer += String((_parameter.getFacility() << 3) | _parameter.getSeverity());
	buffer += '>';
	_addTimestamp(buffer);
	_addParameter(buffer, _parameter.getHostname());
	if (_parameter.getAppName().length()) {
		buffer += _parameter.getAppName();
		if (_parameter.getProcessId().length()) {
			buffer += '[';
			buffer += _parameter.getProcessId();
			buffer += ']';
		}
		buffer += ':';
		buffer += ' ';
	}
#endif
}

bool Syslog::canSend() const {
	return WiFi.isConnected();
}

bool Syslog::isSending() {
	return false;
}

bool Syslog::isNumeric(const char *str) {
    while (*str) {
        if (!isdigit(*str++)) {
            return false;
        }
    }
    return true;
}

SyslogFacility Syslog::facilityToInt(const String &str) {
    if (!isWildcard(str)) {
        if (isNumeric(str.c_str())) {
            return (SyslogFacility)str.toInt();
        }
        for (uint8_t i = 0; syslogFilterFacilityItems[i].value != 0xff; i++) {
            if (strcasecmp_P(str.c_str(), syslogFilterFacilityItems[i].name) == 0) {
                return (SyslogFacility)syslogFilterFacilityItems[i].value;
            }
        }
    }
    return SYSLOG_FACILITY_ANY;
}

SyslogSeverity Syslog::severityToInt(const String &str) {
    if (!isWildcard(str)) {
        if (isNumeric(str.c_str())) {
            return (SyslogSeverity)str.toInt();
        }
        for (uint8_t i = 0; syslogFilterSeverityItems[i].value != 0xff; i++) {
            if (strcasecmp_P(str.c_str(), syslogFilterSeverityItems[i].name) == 0) {
                return (SyslogSeverity)syslogFilterSeverityItems[i].value;
            }
        }
    }
    return SYSLOG_SEVERITY_ANY;
}
