/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Syslog.h"

class SyslogUDP : public Syslog {
public:
    static constexpr uint16_t kDefaultPort = 514;

    SyslogUDP(SyslogParameter &&parameter, SyslogQueue &queue, const String &host, uint16_t port = kDefaultPort);
    virtual ~SyslogUDP();

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port);
    virtual bool canSend() const;
    virtual bool isSending();
    virtual void transmit(const SyslogQueueItem &item);
    virtual String getHostname() const;
    virtual uint16_t getPort() const;

private:
    char *_host;
    IPAddress _address;
    uint16_t _port;
    WiFiUDP _udp;
};
