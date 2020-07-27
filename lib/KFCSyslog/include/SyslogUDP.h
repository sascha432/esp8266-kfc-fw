/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class SyslogUDP : public Syslog {
public:
    static constexpr uint16_t kDefaultPort = 514;

    SyslogUDP(SyslogParameter &parameter, const String &host, uint16_t port = kDefaultPort);

    void transmit(const String &message, Callback_t callback) override;

private:
    String _host;
    uint16_t _port;
    WiFiUDP _udp;
};
