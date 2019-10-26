/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include "Serial2TcpBase.h"

class Serial2TcpClient : public Serial2TcpBase {
public:
    Serial2TcpClient(Stream &serial, uint8_t serialPort);
    virtual ~Serial2TcpClient();

    static Serial2TcpClient *getInstance() {
        return getInstance();
    }

    virtual void getStatus(PrintHtmlEntitiesString &output);

    void setAutoConnect(bool autoConnect) {
        _autoConnect = autoConnect;
    }
    bool getAutoConnect() const {
        return _autoConnect;
    }

    void setAutoReconnect(uint8_t autoReconnect) {
        _autoReconnect = autoReconnect;
    }
    uint8_t getAutoReconnect() const {
        return _autoReconnect;
    }

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
    uint8_t _autoConnect: 1;
    uint8_t _autoReconnect;
    Serial2TcpConnection *_conn;
    EventScheduler::TimerPtr _timer;
};

#endif
