/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogParameter::SyslogParameter(SyslogParameter &&move) :
    _facility(move._facility),
    _severity(move._severity),
    _hostname(std::exchange(move._hostname, nullptr)),
    _appName(move._appName),
    _processId(std::exchange(move._processId, nullptr))
{
    __LDBG_printf("hostname=%s appname=%s procid=%s", __S(_hostname), __S(_appName), __S(_processId));
}

SyslogParameter::SyslogParameter(const char *hostname, const __FlashStringHelper *appName, const char *processId) :
    _facility(SYSLOG_FACILITY_KERN),
    _severity(SYSLOG_NOTICE),
    _hostname(strdup(hostname)),
    _appName(appName),
    _processId(processId ? strdup(processId) : nullptr)
{
    __LDBG_printf("hostname=%s appname=%s procid=%s", __S(_hostname), __S(_appName), __S(_processId));
}

SyslogParameter::~SyslogParameter()
{
    if (_processId) {
        free(_processId);
    }
    if (_hostname) {
        free(_hostname);
    }
}

void SyslogParameter::setFacility(SyslogFacility facility)
{
    _facility = facility;
}

SyslogFacility SyslogParameter::getFacility() const
{
    return _facility;
}

void SyslogParameter::setSeverity(SyslogSeverity severity)
{
    _severity = severity;
}

SyslogSeverity SyslogParameter::getSeverity() const
{
    return _severity;
}

void SyslogParameter::setAppName(const __FlashStringHelper *appName)
{
    _appName = appName;
    __LDBG_printf("appname=%s", __S(_appName));
}

const __FlashStringHelper *SyslogParameter::getAppName() const
{
    return _appName;
}

void SyslogParameter::setHostname(const char *hostname)
{
    if (_hostname) {
        free(_hostname);
    }
    _hostname = strdup(hostname);
    __LDBG_printf("hostname=%s", __S(_hostname));
}

const char *SyslogParameter::getHostname() const
{
    return _hostname;
}

void SyslogParameter::setProcessId(const char *processId)
{
    if (_processId) {
        free(_processId);
        _processId = nullptr;
    }
    if (processId) {
        _processId = strdup(processId);
    }
    __LDBG_printf("procid=%s", __S(_processId));
}

const char *SyslogParameter::getProcessId() const
{
    return _processId;
}
