/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogUDP.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

SyslogUDP::SyslogUDP(SyslogParameter& parameter, const char* host, uint16_t port) : Syslog(parameter) {
    _host = host;
    _port = port;
    _debug_printf_P(PSTR("SyslogUDP::SyslogUDP %s:%d\n"), host, port);
}

void SyslogUDP::transmit(const char* message, size_t length, SyslogCallback callback) {
#if DEBUG_SYSLOG
    if (!strstr_P(message, PSTR("SyslogUDP::transmit"))) {
        _debug_printf_P(PSTR("SyslogUDP::transmit '%s' length %d\n"), message, length);
    }
#endif

    bool success = _udp.beginPacket(_host.c_str(), _port) && _udp.write((const uint8_t*)message, length) == length && _udp.endPacket();
    callback(success);
}
