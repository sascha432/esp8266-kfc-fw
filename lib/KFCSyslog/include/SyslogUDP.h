/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define SYSLOG_PORT_UDP 514

class SyslogUDP : public Syslog {
public:
    SyslogUDP(SyslogParameter &parameter, const String &host, uint16_t port = SYSLOG_PORT_UDP);

    void transmit(const String &message, Callback_t callback) override;

private:
    String _host;
    uint16_t _port;
    WiFiUDP _udp;
};
