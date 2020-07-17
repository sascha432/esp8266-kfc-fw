/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "Serial2TcpBase.h"

class Serial2TcpClient : public Serial2TcpBase {
public:
    Serial2TcpClient(Stream &serial, const char *hostname, const Serial2TCP::Serial2Tcp_t &config);
    virtual ~Serial2TcpClient();

    static Serial2TcpClient *getInstance() {
        return getInstance();
    }

    virtual void getStatus(Print &output);

    virtual void begin();
    virtual void end();

    virtual void _onSerialData(uint8_t type, const uint8_t *buffer, size_t len) override;
    virtual void _onData(AsyncClient *client, void *data, size_t len) override;
    // virtual size_t _serialWrite(Serial2TcpConnection *conn, const char *data, size_t len) override;
    virtual void _onDisconnect(AsyncClient *client, const __FlashStringHelper *reason) override;

private:
    void _connect();
    void _disconnect();

private:
    Serial2TcpConnection *_conn;
    EventScheduler::Timer _timer;
};
