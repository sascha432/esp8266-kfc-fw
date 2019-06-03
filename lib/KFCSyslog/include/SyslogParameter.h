/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

enum SyslogSeverity {
    SYSLOG_EMERG = 0,
    SYSLOG_ALERT,
    SYSLOG_CRIT,
    SYSLOG_ERR,
    SYSLOG_WARN,
    SYSLOG_NOTICE,
    SYSLOG_INFO,
    SYSLOG_DEBUG,
    SYSLOG_SEVERITY_ANY = 0xff,
};

typedef uint8_t SyslogFacility;

enum {
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
    SYSLOG_FACILITY_ANY = 0xff,
};

class SyslogParameter {
   public:
    SyslogParameter();
    SyslogParameter(const char *hostname, const char *appName, const char *processId);
    SyslogParameter(const String hostname, const String appName);
    SyslogParameter(const String hostname, const String appName, const String processId);

    void setFacility(SyslogFacility facility);
    SyslogFacility getFacility();

    void setSeverity(SyslogSeverity severity);
    SyslogSeverity getSeverity();

    void setHostname(const char *hostname);
    void setHostname(const String hostname);
    const String &getHostname();

    void setAppName(const char *appName);
    void setAppName(const String appName);
    const String &getAppName();

    void setProcessId(const char *processId);
    void setProcessId(const String processId);
    const String &getProcessId();

   private:
    SyslogFacility _facility;
    SyslogSeverity _severity;
    String _hostname;
    String _appName;
    String _processId;
};
