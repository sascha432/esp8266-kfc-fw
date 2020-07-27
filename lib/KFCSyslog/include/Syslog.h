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

class Syslog;

#if defined(HAVE_KFC_FIRMWARE_VERSION) && HAVE_KFC_FIRMWARE_VERSION

#include "kfc_fw_config_types.h"

using SyslogProtocol = SyslogProtocolType;

#else

enum class SyslogProtocol {
    MIN = 0,
    NONE = MIN,
    UDP,
    TCP,
    TCP_TLS,
    FILE,
    MAX
};

#endif


class SyslogFilterItem {
public:
    SyslogFilterItem(SyslogFacility facility, SyslogSeverity severity) {
        _facility = facility;
        _severity = severity;
    }
    SyslogFilterItem(const String &facility, const String &severity);

    bool isMatch(SyslogFacility facility, SyslogSeverity severity) const {
        return ((_facility == SYSLOG_FACILITY_ANY || _facility == facility) && (_severity == SYSLOG_SEVERITY_ANY ||_severity == severity));
    }

private:
    SyslogFacility _facility;
    SyslogSeverity _severity;
};

typedef std::vector<SyslogFilterItem> SyslogFilterItemVector;

class SyslogFileFilterItem {
public:
    SyslogFileFilterItem() : _syslog(nullptr) {
    }
    SyslogFileFilterItem(const String &filter, Syslog *_syslog);

    bool isStop() const;
    bool isMatch(SyslogFacility facility, SyslogSeverity severity);

    Syslog *getSyslog() const {
        return _syslog;
    }

public:
    static void _parseFilter(const String &filter, SyslogFilterItemVector &filters);

private:
    SyslogFilterItemVector _filter;
    Syslog *_syslog;
};

// facility/severity name/value pairs
typedef struct {
    PGM_P name;
    uint8_t value;
} SyslogFilterItemPair;

extern const SyslogFilterItemPair syslogFilterFacilityItems[] PROGMEM;
extern const SyslogFilterItemPair syslogFilterSeverityItems[] PROGMEM;

class Syslog {
public:
    typedef std::function<void(bool success)> Callback_t;

    Syslog(SyslogParameter &parameter);
    virtual ~Syslog() {}

    virtual void transmit(const String &message, Callback_t callback);

    virtual void addHeader(String &buffer);

    // able to send?
    virtual bool canSend() const;
    // busy sending?
	virtual bool isSending();

	static bool isNumeric(const char *str);
    static inline bool isWildcard(const String &string) {
        return string.length() == 1 && string.charAt(0) == '*';
    }

	static SyslogFacility facilityToInt(const String &str);
	static SyslogSeverity severityToInt(const String &str);

protected:
    void _addTimestamp(String &buffer, PGM_P format = SYSLOG_TIMESTAMP_FORMAT);
    void _addParameter(String &buffer, const String &value);

    SyslogParameter &_parameter;
};
