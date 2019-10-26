/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

#pragma once

#include <Arduino_compat.h>
#include "Serial2TcpBase.h"

class Serial2TcpServer : public Serial2TcpBase {
public:
    typedef std::unique_ptr<Serial2TcpConnection> Serial2TcpConnectionPtr;
    typedef std::vector<Serial2TcpConnectionPtr> ConnectionsVector;

    Serial2TcpServer(Stream &serial, uint8_t serialPort);
    virtual ~Serial2TcpServer();

    static Serial2TcpServer *getInstance() {
        return getInstance();
    }

    virtual void getStatus(PrintHtmlEntitiesString &output);

    virtual void begin();
    virtual void end();

public:
    static void handleNewClient(void *arg, AsyncClient *client);

private:
    String _getClientInfo(Serial2TcpConnection &conn) const;
    Serial2TcpConnection *_getConn(AsyncClient *client);

    void _handleNewClient(AsyncClient *client);
    virtual void _onSerialData(uint8_t type, const uint8_t *buffer, size_t len) override;
    virtual void _onData(AsyncClient *client, void *data, size_t len) override;
    virtual void _onDisconnect(AsyncClient *client, const __FlashStringHelper *reason) override;
    virtual size_t _serialWrite(Serial2TcpConnection *conn, const char *data, size_t len) override;

    Serial2TcpConnection &_addClient(AsyncClient *client);
    void _removeClient(AsyncClient *client);

    // callback for first client connected
    void _onStart();
    // last client disconnected
    void _onEnd();

private:
    ConnectionsVector _connections;
    AsyncServer *_server;
};

#endif
