/**
 * Author: sascha_lammers@gmx.de
 */

#include "SyslogTCP.h"

SyslogTCP::SyslogTCP(SyslogParameter& parameter, const char* host, uint16_t port, bool useTLS) : Syslog(parameter) {
    _host = host;
    _port = port;
    _useTLS = useTLS;
}
