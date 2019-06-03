/**
 * Author: sascha_lammers@gmx.de
 */

#include "SyslogUDP.h"

SyslogUDP::SyslogUDP(SyslogParameter& parameter, const char* host, uint16_t port) : Syslog(parameter) {
    _host = host;
    _port = port;
}

void SyslogUDP::transmit(const char* message, size_t length, SyslogCallback callback) {
    if_debug_printf_P(PSTR("SyslogUDP::transmit '%s' length %d\n"), message, length);

    bool success = _udp.beginPacket(_host.c_str(), _port) && _udp.write((const uint8_t*)message, length) == length && _udp.endPacket();
    callback(success);
}
