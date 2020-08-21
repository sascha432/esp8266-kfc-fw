/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Buffer.h>
#include "Syslog.h"

#if defined(ESP8266)
#include <ESPAsyncTCP.h>
#elif defined(ESP32)
#include <AsyncTCP.h>
#endif

class SyslogTCP : public Syslog {
public:
    static constexpr uint16_t kDefaultPort = 514;
    static constexpr uint16_t kDefaultPortTLS = 6514;
    static constexpr uint16_t kMaxIdleSeconds = 15;

public:
    SyslogTCP(SyslogParameter &&parameter, SyslogQueue &queue, const String &host, uint16_t port = kDefaultPort, bool useTLS = false);
    virtual ~SyslogTCP();

	virtual void transmit(const SyslogQueueItem &item);
    virtual void clear() override;
	virtual bool isSending() override;

public:
    static void _onDisconnect(void *arg, AsyncClient *client);
    static void _onAck(void *arg, AsyncClient *client, size_t len, uint32_t time);
    static void _onError(void *arg, AsyncClient *client, int8_t error);
    static void _onTimeout(void *arg, AsyncClient *client, uint32_t time);
    static void _onPoll(void *arg, AsyncClient *client);

private:
    void __onAck(AsyncClient *client, size_t len, uint32_t time);
    void __onError(AsyncClient *client, int8_t error);
    void __onPoll(AsyncClient *client);

private:
    void _connect();
    void _disconnect();
#if DEBUG_SYSLOG
    void _status(bool success, const char *reason = nullptr);
#else
    void _status(bool success);
#endif

    char *_host;
    IPAddress _address;
    AsyncClient _client;
    Buffer _buffer;                     // data to write for _queueid
    uint32_t _queueId;                  // queue id
    uint32_t _port: 16;
    uint32_t _useTLS: 1;
    uint32_t _ack: 15;                  // number of bytes waiting for ack
};
