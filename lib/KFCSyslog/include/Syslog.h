/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_SYSLOG
#define DEBUG_SYSLOG                            0
#endif

#include <functional>
#include <vector>

#ifndef SYSLOG_USE_RFC5424
#define SYSLOG_USE_RFC5424                      0   // does not seem to be supported by many syslog daemons
#endif
#if SYSLOG_USE_RFC5424
#define SEND_NILVALUE_IF_INVALID_TIMESTAMP      1
#else
#define SEND_NILVALUE_IF_INVALID_TIMESTAMP      0   // if the client cannot determine the time, it is supposed to send a NILVALUE, but this confuses some syslog servers
#endif

#if SYSLOG_USE_RFC5424
#define SYSLOG_VERSION                          "1"
#define SYSLOG_TIMESTAMP_FORMAT                 "%FT%TZ"
#else  // old/fall-back format
#define SYSLOG_TIMESTAMP_FORMAT                 "%h %d %T"
#endif
#define SYSLOG_FILE_TIMESTAMP_FORMAT            "%FT%TZ"

enum SyslogProtocol : uint8_t {
    SYSLOG_PROTOCOL_NONE = 0,
    SYSLOG_PROTOCOL_UDP,
    SYSLOG_PROTOCOL_TCP,
    SYSLOG_PROTOCOL_TCP_TLS,
    SYSLOG_PROTOCOL_FILE,
};

typedef std::vector<std::pair<SyslogFacility, SyslogSeverity>> SyslogFilterItemVector;

typedef std::function<void(bool success)> SyslogCallback;

class Syslog;

struct SyslogFileFilterItem {
    SyslogFilterItemVector filter;
    Syslog *syslog;
};

// facility/severity name/value pairs
struct SyslogFilterItem {
    PGM_P name;
    uint8_t value;
};

extern const SyslogFilterItem syslogFilterFacilityItems[] PROGMEM;
extern const SyslogFilterItem syslogFilterSeverityItems[] PROGMEM;

class Syslog {
public:
    Syslog(SyslogParameter &parameter);

    virtual void transmit(const String &message, SyslogCallback callback);

    virtual void addHeader(String &buffer);
    virtual bool canSend() const;
	virtual bool isSending();

	static bool isNumeric(const char *str);
	static SyslogFacility facilityToInt(const String &str);
	static SyslogSeverity severityToInt(const String &str);

protected:
    void _addTimestamp(String &buffer, PGM_P format = SYSLOG_TIMESTAMP_FORMAT);
    void _addParameter(String &buffer, const String &value);

    SyslogParameter &_parameter;
};
