/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef SYSLOG_APPNAME
#define SYSLOG_APPNAME                          "kfcfw"
#endif

typedef enum  : uint8_t {
    SYSLOG_EMERG = 0,
    SYSLOG_ALERT,
    SYSLOG_CRIT,
    SYSLOG_ERR,
    SYSLOG_WARN,
    SYSLOG_NOTICE,
    SYSLOG_INFO,
    SYSLOG_DEBUG,
    SYSLOG_SEVERITY_MAX
} SyslogSeverity;

typedef enum : uint8_t {
    SYSLOG_FACILITY_KERN = 0,
    SYSLOG_FACILITY_USER,
    SYSLOG_FACILITY_MAIL,
    SYSLOG_FACILITY_DAEMON,
    SYSLOG_FACILITY_SECURE,
    SYSLOG_FACILITY_SYSLOG,
    SYSLOG_FACILITY_NTP = 12,
    SYSLOG_FACILITY_LOCAL0 = 16,
    SYSLOG_FACILITY_LOCAL1,
    SYSLOG_FACILITY_LOCAL2,
    SYSLOG_FACILITY_LOCAL3,
    SYSLOG_FACILITY_LOCAL4,
    SYSLOG_FACILITY_LOCAL5,
    SYSLOG_FACILITY_LOCAL6,
    SYSLOG_FACILITY_LOCAL7,
    SYSLOG_FACILITY_MAX,
} SyslogFacility;

class SyslogParameter {
public:
    SyslogParameter(SyslogParameter &&) = delete;
    SyslogParameter(const SyslogParameter &) = delete;

    SyslogParameter(const char *hostname);
    ~SyslogParameter();

    void setFacility(SyslogFacility facility);
    SyslogFacility getFacility() const;

    void setSeverity(SyslogSeverity severity);
    SyslogSeverity getSeverity() const;

    void setHostname(const char *hostname);
    const char *getHostname() const;

private:
    SyslogFacility _facility;
    SyslogSeverity _severity;
    char *_hostname;
};

inline SyslogParameter::SyslogParameter(const char *hostname) :
    _facility(SYSLOG_FACILITY_KERN),
    _severity(SYSLOG_NOTICE),
    _hostname(strdup(hostname))
{
}

inline SyslogParameter::~SyslogParameter()
{
    if (_hostname) {
        free(_hostname);
    }
}

inline void SyslogParameter::setFacility(SyslogFacility facility)
{
    _facility = facility;
}

inline SyslogFacility SyslogParameter::getFacility() const
{
    return _facility;
}

inline void SyslogParameter::setSeverity(SyslogSeverity severity)
{
    _severity = severity;
}

inline SyslogSeverity SyslogParameter::getSeverity() const
{
    return _severity;
}

inline void SyslogParameter::setHostname(const char *hostname)
{
    if (_hostname) {
        free(_hostname);
    }
    _hostname = strdup(hostname);
}

inline const char *SyslogParameter::getHostname() const
{
    return _hostname;
}
