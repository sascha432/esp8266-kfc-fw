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
    static void broadcast(WsWebUISocket *sender, const uint8_t *buf, size_t len);
    inline static void broadcast(WsWebUISocket *sender, const char *str) {
        broadcast(sender, reinterpret_cast<const uint8_t *>(str), strlen(str));
    }
    inline static void broadcast(WsWebUISocket *sender, const String &str) {
        broadcast(sender, reinterpret_cast<const uint8_t *>(str.c_str()), str.length());
    }
    inline static void broadcast(WsWebUISocket *sender, const __FlashStringHelper *str) {
        broadcast(sender, reinterpret_cast<const uint8_t *>(str), strlen(reinterpret_cast<PGM_P>(str)));
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

using WebUISocket = WsWebUISocket;
