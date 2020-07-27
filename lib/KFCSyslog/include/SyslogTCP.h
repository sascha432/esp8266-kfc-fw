/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Buffer.h>

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

    typedef struct {
        Buffer message;                 // data to write
        uint16_t written;               // bytes written
        uint16_t sentAck;               // bytes sent
        uint8_t isSending: 1;           // currently sending the message
        uint8_t isDisconnecting: 1;     // tcp connection closed
        ulong idleTimeout;              // millis() > timeout = close idle connection
        Callback_t callback;
    } Queue_t;

    SyslogTCP(SyslogParameter &parameter, const String &host, uint16_t port = kDefaultPort, bool useTLS = false);
    ~SyslogTCP();

	void transmit(const String &message, Callback_t callback) override;
	bool isSending() override;

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
    void _disconnect(bool now);
    void _queueWritten(bool ack);
    void _queueWriteError(IF_DEBUG(const char *reason));
    void _queueInvalidState();

    String _host;
    uint16_t _port;
    bool _useTLS;
    Queue_t _queue;
    AsyncClient _client;
};
