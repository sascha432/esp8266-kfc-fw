/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_WEB_SOCKETS
#define DEBUG_WEB_SOCKETS 0
#endif

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <ESPAsyncWebServer.h>

#define WS_PREFIX "ws[%s][%u] "
#define WS_PREFIX_ARGS server->url(), client->id()

class WsClient;

//typedef std::function<WsClient *(WsClient *wsSClient, WsAwsEventType type, AsyncWebSocket *server, AsyncWebSocketClient *client, uint8_t *data, size_t len, void *arg)> WsEventHandlerCallback;
typedef std::function<WsClient *(AsyncWebSocketClient *client)> WsGetInstance;

class WsClient {
public:
    enum WsErrorType {
        ERROR_FROM_SERVER,
        ERROR_AUTHENTTICATION_FAILED,
    };

    WsClient(AsyncWebSocketClient *client);
    virtual ~WsClient();

    void setAuthenticated(bool authenticated);
    bool isAuthenticated() const;

#if WEB_SOCKET_ENCRYPTION
    bool isEncrypted() const;

    void initEncryption(uint8_t *iv, uint8_t *salt);
#endif

    inline void setClient(AsyncWebSocketClient *client) {
        _client = client;
    }
    inline AsyncWebSocketClient *getClient() const {
        return _client;
    }

    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, int type, uint8_t *data, size_t len, void *arg = nullptr, WsGetInstance getInstance = nullptr);

    virtual void onConnect(uint8_t *data, size_t len) {
        // debug_println("WebSocket::onConnect()");
    }
    virtual void onAuthenticated(uint8_t *data, size_t len) {
        // debug_println("WebSocket::onAuthenticated()");
    }
    virtual void onDisconnect(uint8_t *data, size_t len) {
        // debug_println("WebSocket::onDisconnect()");
    }
    virtual void onError(WsErrorType type, uint8_t *data, size_t len) {
        // debug_println("WebSocket::onError()");
    }
    virtual void onPong(uint8_t *data, size_t len) {
        // debug_println("WebSocket::onPong()");
    }
    virtual void onText(uint8_t *data, size_t len) {
        // debug_println("WebSocket::onText()");
    }
    virtual void onBinary(uint8_t *data, size_t len) {
        // debug_println("WebSocket::onBinary()");
    }


    /**
     *  gets called when the first client has been authenticated.
     **/
    virtual void onStart();
    /**
     * gets called once all authenticated clients have been disconnected. onStart() will be called
     * again when authenticated clients become available.
     **/
    virtual void onEnd();

    void onData(AwsFrameType type, uint8_t *data, size_t len);

public:
    // broadcast to all clients except sender, if not null
    static void broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer);
    static void broadcast(AsyncWebSocket *server, WsClient *sender, const char *str, size_t length);
    static void broadcast(AsyncWebSocket *server, WsClient *sender, const __FlashStringHelper *str, size_t length);

    // verify that the client is attached to the server
    static void safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const String &message);

protected:
    static void invokeStartOrEndCallback(WsClient *wsClient, bool isStart);

private:
    bool _authenticated;
    bool _isEncryped;
#if WEB_SOCKET_ENCRYPTION
    uint8_t *_iv;
    uint8_t *_salt;
#endif
    AsyncWebSocketClient *_client;
};
