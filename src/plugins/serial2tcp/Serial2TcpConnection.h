/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#if defined(ESP32)
#include "SyncClient.h"
#else
#include <SoftwareSerial.h>
#include <ESPAsyncTCP.h>
#endif
#include "serial2tcp.h"

class Serial2TcpConnection {
public:
    typedef int8_t error_t;
    typedef int32_t size_t;

    Serial2TcpConnection(AsyncClient *client, bool isAuthenticated);
    virtual ~Serial2TcpConnection();

    void close();

    bool isAuthenticated() const;
    // void setClient(AsyncClient *client);
    AsyncClient *getClient() const;
    Buffer &getTxBuffer();
    Buffer &getRxBuffer();
    Buffer &getNvtBuffer();

private:
    AsyncClient *_client;
    Buffer _txBuffer;
    Buffer _rxBuffer;
    Buffer _nvt;
    bool _isAuthenticated;
};
