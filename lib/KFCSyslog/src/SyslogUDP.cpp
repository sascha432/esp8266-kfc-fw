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

SyslogUDP::SyslogUDP(SyslogParameter& parameter, const String &host, uint16_t port) : Syslog(parameter) {
    _debug_printf_P(PSTR("SyslogUDP::SyslogUDP %s:%d\n"), host.c_str(), port);
    _host = host;
    _port = port;
}

void SyslogUDP::transmit(const String &message, SyslogCallback callback) {
#if DEBUG_SYSLOG
    if (message.indexOf("SyslogUDP::transmit") != - 1) {
        _debug_printf_P(PSTR("SyslogUDP::transmit '%s' length %d\n"), message.c_str(), message.length());
    }
#endif

    bool success = _udp.beginPacket(_host.c_str(), _port) && _udp.write((const uint8_t*)message.c_str(), message.length()) == message.length() && _udp.endPacket();
    callback(success);
}
