/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "Syslog.h"

#define SYSLOG_PORT_TCP 514
#define SYSLOG_PORT_TCP_TLS 6514

class SyslogTCP : public Syslog {
   public:
    SyslogTCP(SyslogParameter &parameter, const char *host, uint16_t port = SYSLOG_PORT_TCP, bool useTLS = false);

   private:
    String _host;
    uint16_t _port;
    bool _useTLS;
};
