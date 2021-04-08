/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Syslog.h"

class SyslogUDP : public Syslog {
public:
    static constexpr uint16_t kDefaultPort = 514;

    SyslogUDP(const char *hostname, SyslogQueue *queue, const String &host, uint16_t port = kDefaultPort);
    virtual ~SyslogUDP();

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port);
    virtual uint32_t getState(StateType state);
    virtual void transmit(const SyslogQueueItem &item);
    virtual String getHostname() const;
    virtual uint16_t getPort() const;

private:
    char *_host;
    IPAddress _address;
    uint16_t _port;
    WiFiUDP _udp;
};

inline SyslogUDP::~SyslogUDP()
{
    if (_host) {
        free(_host);
    }
}

inline uint16_t SyslogUDP::getPort() const
{
    return _port;
}

inline String SyslogUDP::getHostname() const
{
    if (_host) {
        return _host;
    }
    if (IPAddress_isValid(_address)) {
        return _address.toString();
    }
    return emptyString;
}

inline uint32_t SyslogUDP::getState(StateType state)
{
    return (state == StateType::CAN_SEND) && (_port != 0) && (_host || IPAddress_isValid(_address)) && WiFi.isConnected();
}
