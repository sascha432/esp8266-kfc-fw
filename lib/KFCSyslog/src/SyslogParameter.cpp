/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"

SyslogParameter::SyslogParameter() {
    _facility = SYSLOG_FACILITY_KERN;
    _severity = SYSLOG_ERR;
}

SyslogParameter::SyslogParameter(const String &hostname, const String &appName) {
    _hostname = hostname;
    _appName = appName;
}

SyslogParameter::SyslogParameter(const String &hostname, const String &appName, const String &processId) {
    _hostname = hostname;
    _appName = appName;
    _processId = processId;
}

void SyslogParameter::setFacility(SyslogFacility facility) {
    _facility = facility;
}

SyslogFacility SyslogParameter::getFacility() {
    return _facility;
}

void SyslogParameter::setSeverity(SyslogSeverity severity) {
    _severity = severity;
}

SyslogSeverity SyslogParameter::getSeverity() {
    return _severity;
}

void SyslogParameter::setAppName(const String &appName) {
    _appName = appName;
}

const String& SyslogParameter::getAppName() {
    return _appName;
}

void SyslogParameter::setHostname(const String &hostname) {
    _hostname = hostname;
}

const String& SyslogParameter::getHostname() {
    return _hostname;
}

void SyslogParameter::setProcessId(const String &processId) {
    _processId = processId;
}

const String& SyslogParameter::getProcessId() {
    return _processId;
}
