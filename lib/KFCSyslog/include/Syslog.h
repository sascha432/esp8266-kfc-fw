/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <functional>
#include <vector>

// #define if_debug_printf_P

#define USE_RFC5424 0  // does not seem to be supported by many syslog daemons

#if USE_RFC5424
#define SYSLOG_VERSION "1"
#define SYSLOG_TIMESTAMP_FORMAT "%FT%TZ"
#else  // old/fall-back format
#define SYSLOG_TIMESTAMP_FORMAT "%h %d %T"
#endif
#define SYSLOG_FILE_TIMESTAMP_FORMAT "%FT%TZ"

enum SyslogProtocol {
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

extern SyslogFilterItem syslogFilterFacilityItems[];
extern SyslogFilterItem syslogFilterSeverityItems[];

class Syslog {
   public:
    Syslog(SyslogParameter &parameter);

    void transmit(const String message, SyslogCallback callback) {
        transmit(message.c_str(), message.length(), callback);
    }
    virtual void transmit(const char *message, size_t length, SyslogCallback callback);

    virtual void addHeader(String &buffer);
    virtual bool canSend();
	virtual bool isSending();

	static bool isNumeric(const char *str);
	static SyslogFacility facilityToInt(const String str);
	static SyslogSeverity severityToInt(const String str);

   protected:
    void _addTimestamp(String &buffer, PGM_P format = SYSLOG_TIMESTAMP_FORMAT);
    void _addParameter(String &buffer, const String &value);

    SyslogParameter &_parameter;
};
