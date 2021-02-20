/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "WebUIComponent.h"
#include "AsyncWebSocket.h"
#include "web_server.h"
#include "web_socket.h"
#include "templates.h"

class WsWebUISocket : public WsClient {
public:
    using WsClient::WsClient;
    using WsClient::hasClients;

public:
    static WsClient *createInstance(AsyncWebSocketClient *socket);

    virtual void onText(uint8_t *data, size_t len) override;

public:
    static void send(AsyncWebSocketClient *client, const JsonUnnamedObject &json);
    static void broadcast(WsWebUISocket *sender, const JsonUnnamedObject &json);
    // buf is an allocated (new uint8_t[len + 1]) null terminated string
    // len = strlen(buf)
    static void broadcast(WsWebUISocket *sender, uint8_t *buf, size_t len);
    static void broadcast(WsWebUISocket *sender, const __FlashStringHelper *str) {
        auto message = reinterpret_cast<uint8_t *>(const_cast<__FlashStringHelper *>(str));
        size_t len = strlen(reinterpret_cast<PGM_P>(str));
        broadcast(sender, message, len);
    }
    static void setup();

    static void createWebUIJSON(JsonUnnamedObject &json);
    static void sendValues(AsyncWebSocketClient *client);

    static WsWebUISocket *getSender();

    static bool hasAuthenticatedClients();
    static WsClientAsyncWebSocket *getServerSocket();


private:
    static WsWebUISocket *_sender;
};
