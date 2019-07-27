/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Buffer.h>

#if defined(ESP8266)
#include <ESPAsyncTCP.h>
#elif defined(ESP32)
#endif

#define SYSLOG_PORT_TCP                 514
#define SYSLOG_PORT_TCP_TLS             6514

// Idle time before closing the socket
#define SYSLOG_TCP_MAX_IDLE             15

class SyslogTCP : public Syslog {
public:
    typedef struct {
        Buffer message;                 // data to write
        uint16_t written;               // bytes written
        uint16_t sentAck;               // bytes sent
        uint8_t isSending: 1;           // currently sending the message
        uint8_t isDisconnecting: 1;     // tcp connection closed
        ulong idleTimeout;              // millis() > timeout = close idle connection
        SyslogCallback callback;
    } Queue_t;

    SyslogTCP(SyslogParameter &parameter, const String &host, uint16_t port = SYSLOG_PORT_TCP, bool useTLS = false);
    ~SyslogTCP();

	void transmit(const String &message, SyslogCallback callback) override;
	bool isSending() override;

public:
    // TODO remove unused callbacks
    static void _onConnect(void *arg, AsyncClient *client);
    static void _onDisconnect(void *arg, AsyncClient *client);
    static void _onAck(void *arg, AsyncClient *client, size_t len, uint32_t time);
    static void _onError(void *arg, AsyncClient *client, int8_t error);
    static void _onData(void *arg, AsyncClient *client, void *data, size_t len);
    static void _onPacket(void *arg, AsyncClient *client, struct pbuf *pb);
    static void _onTimeout(void *arg, AsyncClient *client, uint32_t time);
    static void _onPoll(void *arg, AsyncClient *client);

private:
    // void __onConnect(AsyncClient *client);
    // void __onDisconnect(AsyncClient *client);
    void __onAck(AsyncClient *client, size_t len, uint32_t time);
    void __onError(AsyncClient *client, int8_t error);
    // void __onData(AsyncClient *client, void *data, size_t len);
    // void __onPacket(AsyncClient *client, struct pbuf *pb);
    // void __onTimeout(AsyncClient *client, uint32_t time);
    void __onPoll(AsyncClient *client);
    void _eventHandler(const char *name, AsyncClient *client, void *data);

private:
    void _connect();
    void _disconnect(bool now);
    bool _isConnected();
    void _queueWritten(bool ack);
    void _queueWriteError();
    void _queueInvalidState();

    String _host;
    uint16_t _port;
    bool _useTLS;

    uint8_t _error;
    Queue_t _queue;

    AsyncClient _client;
};
