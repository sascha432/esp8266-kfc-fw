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

class WebUISocket : public WsClient {
public:
    using WsClient::WsClient;
    using WsClient::hasClients;

public:
    inline static WsClient *createInstance(AsyncWebSocketClient *socket) {
        return __LDBG_new(WebUISocket, socket);
    }

    virtual void onText(uint8_t *data, size_t len) override;

public:
    // static void send(AsyncWebSocketClient *client, const JsonUnnamedObject &json) {
    //     WsClient::safeSend(getServerSocket(), client, json);
    // }

    static void send(AsyncWebSocketClient *client, const MQTT::Json::UnnamedObject &json) {
        WsClient::safeSend(getServerSocket(), client, json);
    }

    // text message with no encoding
    inline static void broadcast(WebUISocket *sender, const uint8_t *str, size_t len) {
        WsClient::broadcast(getServerSocket(), sender, str, len);
    }

    // inline static void broadcast(WebUISocket *sender, const JsonUnnamedObject &json) {
    //     WsClient::broadcast(getServerSocket(), sender, json);
    // }

    // text message UTF-8
    inline static void broadcast(WebUISocket *sender, const char *str) {
        WsClient::broadcast(getServerSocket(), sender, str, strlen(str));
    }
    inline static void broadcast(WebUISocket *sender, String &&str) {
        WsClient::broadcast(getServerSocket(), sender, std::move(str));
    }
    inline static void broadcast(WebUISocket *sender, const String &str) {
        WsClient::broadcast(getServerSocket(), sender, str.c_str(), str.length());
    }
    inline static void broadcast(WebUISocket *sender, const MQTT::Json::UnnamedObject &json) {
        WsClient::broadcast(getServerSocket(), sender, json);
    }
    // text message with no encoding
    inline static void broadcast(WebUISocket *sender, const __FlashStringHelper *str) {
        WsClient::broadcast(getServerSocket(), sender, str, strlen(reinterpret_cast<PGM_P>(str)));
    }

    static void setup(AsyncWebServer *server);

    static WebUINS::Root createWebUIJSON();
    static void sendValues(AsyncWebSocketClient *client);

    inline static WebUISocket *getSender() {
        return _sender;
    }

    inline static bool hasAuthenticatedClients() {
        return _server && _server->hasAuthenticatedClients();
    }
    inline static WsClientAsyncWebSocket *getServerSocket() {
        return _server;
    }


private:
    static WebUISocket *_sender;
    static WsClientAsyncWebSocket *_server;
};
