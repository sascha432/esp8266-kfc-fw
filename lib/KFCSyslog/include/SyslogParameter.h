/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

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
    SyslogParameter(SyslogParameter &&move);
    SyslogParameter(const SyslogParameter &) = delete;

    SyslogParameter(const char *hostname, const __FlashStringHelper *appName = nullptr, const char *processId = nullptr);
    ~SyslogParameter();

    void setFacility(SyslogFacility facility);
    SyslogFacility getFacility() const;

    void setSeverity(SyslogSeverity severity);
    SyslogSeverity getSeverity() const;

    void setHostname(const char *hostname);
    const char *getHostname() const;

    void setAppName(const __FlashStringHelper *appName = nullptr);
    const __FlashStringHelper *getAppName() const;

    void setProcessId(const char *processId = nullptr);
    const char *getProcessId() const;

private:
    SyslogFacility _facility;
    SyslogSeverity _severity;
    char *_hostname;
    const __FlashStringHelper *_appName;
    char *_processId;
};
