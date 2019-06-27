/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "SyslogParameter.h"
#include "Syslog.h"
#include "SyslogUDP.h"

SyslogUDP::SyslogUDP(SyslogParameter& parameter, const char* host, uint16_t port) : Syslog(parameter) {
    _host = host;
    _port = port;
    debug_printf_P(PSTR("SyslogUDP::SyslogUDP %s:%d\n"), host, port);
}

void SyslogUDP::transmit(const char* message, size_t length, SyslogCallback callback) {
#if DEBUG
    if (!strstr_P(message, PSTR("SyslogUDP::transmit"))) {
        debug_printf_P(PSTR("SyslogUDP::transmit '%s' length %d\n"), message, length);
    }
#endif

    bool success = _udp.beginPacket(_host.c_str(), _port) && _udp.write((const uint8_t*)message, length) == length && _udp.endPacket();
    callback(success);
}
