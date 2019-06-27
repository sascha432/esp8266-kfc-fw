/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogTCP.h"

SyslogTCP::SyslogTCP(SyslogParameter& parameter, const char* host, uint16_t port, bool useTLS) : Syslog(parameter) {
    _host = host;
    _port = port;
    _useTLS = useTLS;
    debug_printf_P(PSTR("SyslogTCP::SyslogTCP %s:%d TLS %d\n"), host, port, useTLS);
}

void SyslogTCP::transmit(const char * message, size_t length, SyslogCallback callback) {
}

bool SyslogTCP::isSending() {
	 return false;
}
