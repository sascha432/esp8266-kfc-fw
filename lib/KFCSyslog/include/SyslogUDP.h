/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "Syslog.h"

#define SYSLOG_PORT_UDP 514

class SyslogUDP : public Syslog {
   public:
    SyslogUDP(SyslogParameter &parameter, const char *host, uint16_t port = SYSLOG_PORT_UDP);

    void transmit(const char *message, size_t length, SyslogCallback callback) override;

   private:
    String _host;
    uint16_t _port;
    WiFiUDP _udp;
};
