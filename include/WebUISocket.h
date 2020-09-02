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

    static WsClient *getInstance(AsyncWebSocketClient *socket);

    virtual void onText(uint8_t *data, size_t len) override;

    static void send(AsyncWebSocketClient *client, const JsonUnnamedObject &json);
    static void broadcast(WsWebUISocket *sender, const JsonUnnamedObject &json);
    static void setup();

    static void createWebUIJSON(JsonUnnamedObject &json);
    static void sendValues(AsyncWebSocketClient *client);

    static WsWebUISocket *getSender();
    static WsClientAsyncWebSocket *getWsWebUI();

private:
    static WsWebUISocket *_sender;
};
