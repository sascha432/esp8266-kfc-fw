/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Buffer.h>
#include <EventScheduler.h>
#include "Syslog.h"

#if defined(ESP8266)
#include <ESPAsyncTCP.h>
#elif defined(ESP32)
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif

class SyslogTCP : public Syslog {
public:
    static constexpr uint16_t kDefaultPort = 514;
    static constexpr uint16_t kDefaultPortTLS = 6514;
    static constexpr uint16_t kMaxIdleSeconds = 30;
    static constexpr uint16_t kReconnectDelay = 3000;

public:
    SyslogTCP(const char *hostname, SyslogQueue *queue, const String &host, uint16_t port = kDefaultPort, bool useTLS = false);
    virtual ~SyslogTCP();

    virtual bool setupZeroConf(const String &hostname, const IPAddress &address, uint16_t port);
	virtual void transmit(const SyslogQueueItem &item);
    virtual uint32_t getState(StateType state);
    virtual void clear() override;
    virtual String getHostname() const;
    virtual uint16_t getPort() const;

public:
    inline static void __onPoll(void *arg, AsyncClient *client) {
        reinterpret_cast<SyslogTCP *>(arg)->_sendQueue();
    }

private:
    void _onAck(size_t len, uint32_t time);
    void _onError(int8_t error);
    void _onTimeout(uint32_t time);
    void _onDisconnect();
    void _onPoll();
    void _sendQueue();

private:
    // calls _connect() after 3 seconds
    // subsequent calls do not increase the delay
    void _reconnect();
    // create new client and connect
    void _connect();
    // disconnect gracefully
    void _disconnect();
    // status report
    void _status(bool success, const __FlashStringHelper *reason = nullptr);

    // create (new) client
    void _allocClient();
    // destroy client
    void _freeClient();

    bool hasQueue() const;
    bool hasNoQueue() const;

private:
    AsyncClient *_client;
    Event::Timer _reconnectTimer;
    char *_host;
    IPAddress _address;
    Buffer _buffer;                     // data to write for _queueid
    uint32_t _queueId;                  // queue id
    uint16_t _port;
    uint16_t _ack: 15;                  // number of bytes waiting for ack
    uint16_t _useTLS: 1;
};

inline void SyslogTCP::_reconnect()
{
    __DBG_printf("reconnect in %ums", kReconnectDelay);
    if (!_reconnectTimer) {
        // add new timer
        _Timer(_reconnectTimer).add(Event::milliseconds(kReconnectDelay), false, [this](Event::CallbackTimerPtr) {
            __DBG_printf("reconnecting");
            _connect();
        });
    }
}

inline SyslogTCP::~SyslogTCP()
{
    _reconnectTimer.remove();
    _queueId = 0;
    _freeClient();
    if (_host) {
        free(_host);
        _host = nullptr;
    }
}

inline String SyslogTCP::getHostname() const
{
    return _host ? _host : (IPAddress_isValid(_address) ? _address.toString() : emptyString);
}

inline uint16_t SyslogTCP::getPort() const
{
    return _port;
}

inline void SyslogTCP::_disconnect()
{
    _reconnectTimer.remove();
    _client->close();
}

inline bool SyslogTCP::hasQueue() const
{
    return _queueId != 0;
}

inline bool SyslogTCP::hasNoQueue() const
{
    return _queueId == 0;
}
