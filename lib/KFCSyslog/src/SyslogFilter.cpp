/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogUDP.h"
#include "SyslogTCP.h"
#include "SyslogFile.h"
#include "SyslogFilter.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogFilter::SyslogFilter(const SyslogParameter &parameter)
{
    _parameter = parameter;
}

SyslogFilter::SyslogFilter(const String &hostname, const String &appName) : _parameter(hostname, appName)
{
}

void SyslogFilter::addFilter(const String &filter, const String &destination)
{
    _filters.push_back(SyslogFileFilterItem(filter, createSyslogFromString(destination)));
}

void SyslogFilter::addFilter(const String &filter, Syslog* syslog)
{
    _filters.push_back(SyslogFileFilterItem(filter, syslog));
}

SyslogParameter& SyslogFilter::getParameter()
{
    return _parameter;
}

void SyslogFilter::applyFilters(SyslogFilterCallback callback)
{
    for(auto filter: _filters) {
        if (filter.isMatch(_parameter.getFacility(), _parameter.getSeverity())) {
            if (filter.isStop()) {
                break;
            }
            callback(filter);
        }
    }
}

Syslog* SyslogFilter::createSyslogFromString(const String &str)
{
    char* tok[4];
    uint8_t tok_count = 0;
    Syslog* syslog = nullptr;

    _debug_printf_P(PSTR("SyslogFilter::createSyslogFromString(%s)\n"), str.c_str());

    for (auto item = _syslogObjects.begin(); item != _syslogObjects.end(); ++item) {
        if (item->first == str) {
            return item->second;
        }
    }

    std::unique_ptr<char []> _strCopy(new char[str.length() + 1]);
    char* ptr = strcpy(_strCopy.get(), str.c_str());

    const char* sep = ":";
    tok[tok_count] = strtok(ptr, sep);
    while (tok[tok_count++] && tok_count < 3) {
        tok[tok_count] = strtok(nullptr, sep);
    }
    if (tok_count > 1) {
        tok[tok_count] = nullptr;

        ptr = tok[0];
        if (*ptr++ == '@') {
            if (*ptr == '@') {  // TCP
                bool useTLS = false;
                if (*++ptr == '!') {  // TLS over TCP
                    useTLS = true;
                    ptr++;
                }
                syslog = _debug_new SyslogTCP(_parameter, ptr, tok[1] ? atoi(tok[1]) : (useTLS ? SYSLOG_PORT_TCP_TLS : SYSLOG_PORT_TCP), useTLS);
            } else if (*ptr) {  // UDP
                syslog = _debug_new SyslogUDP(_parameter, ptr, tok[1] ? atoi(tok[1]) : SYSLOG_PORT_UDP);
            }
        } else if (strcasecmp_P(ptr, PSTR("stop")) == 0) {
            syslog = SYSLOG_FILTER_STOP;
        } else {
            syslog = _debug_new SyslogFile(_parameter, tok[0], tok[1] ? atoi(tok[1]) : SYSLOG_FILE_MAX_SIZE, tok[2] ? atoi(tok[2]) : SYSLOG_FILE_MAX_ROTATE);
        }
    }
    _syslogObjects.push_back(std::make_pair(str, syslog));
    return syslog;
}
