/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "Syslog.h"
#include "SyslogQueue.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if 0

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

#endif

PROGMEM_STRING_DEF(syslog_nil_value, "- ");

Syslog::Syslog(SyslogParameter &&parameter, SyslogQueue &queue) : _parameter(std::move(parameter)), _queue(queue)
{
	__LDBG_printf("host=%s app=%s process=%s queue=%p", __S(_parameter.getHostname()), __S(_parameter.getAppName()), __S(_parameter.getProcessId()), &_queue);
	assert(&_queue != nullptr);
}

Syslog::~Syslog()
{
	__LDBG_delete(reinterpret_cast<SyslogQueue *>(&_queue));
}

void Syslog::_addTimestamp(PrintString &buffer, uint32_t ms, PGM_P format)
{
	time_t now = time(nullptr);
    uint32_t frac = millis() - ms;
#if SEND_NILVALUE_IF_INVALID_TIMESTAMP
	if (!IS_TIME_VALID(now)) {
		buffer.print(FSPGM(syslog_nil_value));
		return;
	}
#endif
    if (ms) {
        now -= frac / 1000;
    }
    auto tm = localtime(&now);
    buffer.strftime_P(format, tm);
#ifdef SYSLOG_TIMESTAMP_FRAC_FMT
    buffer.printf_P(PSTR(SYSLOG_TIMESTAMP_FRAC_FMT), frac);
    char buf[8];
    strftime_P(buf, sizeof(buf), PSTR("%z "), tm);
    buffer.write(buf, 3);
    buffer.write(':');
    buffer.print(buf + 3);
#endif
}

void Syslog::_addParameter(PrintString &buffer, PGM_P value)
{
	if (value) {
        size_t len = strlen_P(value);
        if (len) {
            buffer.write_P((const char *)value, len);
            buffer.write(' ');
            return;
        }
	}
    buffer.print(FSPGM(syslog_nil_value));
}

/*

The syslog message has the following ABNF [RFC5234] definition:

SYSLOG-MSG      = HEADER SP STRUCTURED-DATA [SP MSG]

HEADER          = PRI VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID
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

String Syslog::_getHeader(uint32_t millis)
{
#if SYSLOG_USE_RFC5424

    // currently not working
    //   <3>1 2020-08-21T10:20:38.967-07:00 KFCAC52DB kfcfw - - - It is a test
    //   <5>1 2020-08-21T10:29:09.726-07:00 KFCAC52DB kfcfw - - - MQTT auto discovery published
    // <165>1 2003-08-24T05:14:15.000003-07:00 192.0.2.1 myproc 8710 - - %% It's time to make the do-nuts.
    //  <34>1 2003-10-11T22:14:15.003Z mymachine.example.com appname - ID47 - BOM'su root' failed for lonvick on /dev/pts/8
    // "<" PRIVAL ">" VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP STRUCTURED-DATA [SP MSG]

	PrintString buffer(F("<%u>" SYSLOG_VERSION " "), (_parameter.getFacility() << 3) | _parameter.getSeverity()); // <PRIVAL> VERSION SP
	_addTimestamp(buffer, millis, PSTR(SYSLOG_TIMESTAMP_FORMAT)); // TIMESTAMP SP
	_addParameter(buffer, _parameter.getHostname()); // HOSTNAME SP
	_addParameter(buffer, (PGM_P)_parameter.getAppName()); // APP-NAME SP
	_addParameter(buffer, _parameter.getProcessId()); // PROCID SP
	buffer.print(FSPGM(syslog_nil_value)); // MSGID SP
	buffer.print(FSPGM(syslog_nil_value)); // STRUCTURED-DATA SP
#if SYSLOG_SEND_BOM
    buffer.write(0xef);
    buffer.write(0xbb);
    buffer.write(0xbf);
#endif
	// followed by MSG

#else

	// format used by "rsyslog/stable,now 8.24.0-1 armhf"
	// <PRIVAL> TIMESTAMP SP HOSTNAME SP APPNAME ["[" PROCESSID "]"] ":" SP MESSAGE

	PrintString buffer(F("<%u>"), (_parameter.getFacility() << 3) | _parameter.getSeverity()); // <PRIVAL>
	_addTimestamp(buffer, millis, PSTR(SYSLOG_TIMESTAMP_FORMAT)); // TIMESTAMP SP
	_addParameter(buffer, _parameter.getHostname()); // HOSTNAME SP
    if (_parameter.getAppName()) {
        buffer.print(_parameter.getAppName()); // APP-NAME
        if (_parameter.getProcessId()) {
            buffer.print('[');
	        _addParameter(buffer, _parameter.getProcessId()); // [PROCID]
            buffer.print(']');
        }
        buffer.print(F(": "));
    }
	// followed by MSG

#endif
    return buffer;
}

void Syslog::clear()
{
    __LDBG_print("clear");
    _queue.clear();
}
