/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCTimezone.h>
#include "SyslogParameter.h"
#include "Syslog.h"

SyslogFilterItem syslogFilterFacilityItems[] = {
    {("kern"), SYSLOG_FACILITY_KERN},
    {("user"), SYSLOG_FACILITY_USER},
    {("user"), SYSLOG_FACILITY_USER},
    {("mail"), SYSLOG_FACILITY_MAIL},
    {("daemon"), SYSLOG_FACILITY_DAEMON},
    {("secure"), SYSLOG_FACILITY_SECURE},
    {("syslog"), SYSLOG_FACILITY_SYSLOG},
    {("ntp"), SYSLOG_FACILITY_NTP},
    {("local0"), SYSLOG_FACILITY_LOCAL0},
    {("local1"), SYSLOG_FACILITY_LOCAL1},
    {("local2"), SYSLOG_FACILITY_LOCAL2},
    {("local3"), SYSLOG_FACILITY_LOCAL3},
    {("local4"), SYSLOG_FACILITY_LOCAL4},
    {("local5"), SYSLOG_FACILITY_LOCAL5},
    {("local6"), SYSLOG_FACILITY_LOCAL6},
    {("local7"), SYSLOG_FACILITY_LOCAL7},
    {nullptr, 0xff},
};

SyslogFilterItem syslogFilterSeverityItems[] = {
    {("emerg"), SYSLOG_EMERG},
	{("emergency"), SYSLOG_EMERG},
	{("alert"), SYSLOG_ALERT},
	{("crit"), SYSLOG_CRIT},
	{("critical"), SYSLOG_CRIT},
	{("err"), SYSLOG_ERR},
	{("error"), SYSLOG_ERR},
	{("warn"), SYSLOG_WARN},
	{("warning"), SYSLOG_WARN},
	{("notice"), SYSLOG_NOTICE},
	{("info"), SYSLOG_INFO},
	{("debug"), SYSLOG_DEBUG},
	{nullptr, 0xff},
};

Syslog::Syslog(SyslogParameter &parameter) : _parameter(parameter) {
	debug_printf_P(PSTR("Syslog::Syslog %s,%s,%s\n"), parameter.getHostname().c_str(), parameter.getAppName().c_str(), parameter.getProcessId().c_str());
}

void Syslog::transmit(const char* message, size_t length, SyslogCallback callback) {
	callback(true);
}

void Syslog::_addTimestamp(String& buffer, PGM_P format) {
	auto now = time(nullptr);
	auto tm = timezone_localtime(&now);
	if (tm && IS_TIME_VALID(now)) {
		char buf2[40];
		timezone_strftime_P(buf2, sizeof(buf2), format, tm);
		buffer += buf2;
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

bool Syslog::canSend() {
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

SyslogFacility Syslog::facilityToInt(const String str) {
    if (str.length() != 0 && str != F("*")) {
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

SyslogSeverity Syslog::severityToInt(const String str) {
    if (str.length() != 0 && str != F("*")) {
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
