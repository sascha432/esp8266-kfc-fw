/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_WEB_SOCKETS
#define DEBUG_WEB_SOCKETS               0
#endif

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <ESPAsyncWebServer.h>

#if DEBUG_WEB_SOCKETS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define WS_PREFIX "ws[%s][%u] "
#define WS_PREFIX_ARGS server->url(), client->id()

class WsClient;
class WsClientAsyncWebSocket;
class WebServerPlugin;

//typedef std::function<WsClient *(WsClient *wsSClient, WsAwsEventType type, AsyncWebSocket *server, AsyncWebSocketClient *client, uint8_t *data, size_t len, void *arg)> WsEventHandlerCallback;
typedef std::function<WsClient *(AsyncWebSocketClient *client)> WsGetInstance;

typedef enum : uint16_t {
    RGB565_RLE_COMPRESSED_BITMAP            = 0x0001,
} WebSocketBinaryPacketUnqiueId_t;


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
        __LDBG_printf("data=%s", printable_string(data, len, 32).c_str());
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

public:
    typedef enum {
        CONNECT,
        AUTHENTICATED,
        DISCONNECT,
    } ClientCallbackTypeEnum_t;

    typedef std::function<void(ClientCallbackTypeEnum_t type, WsClient *client)> ClientCallback_t;
    typedef std::vector<ClientCallback_t> ClientCallbackVector_t;

    static void addClientCallback(ClientCallback_t callback);

protected:
    static void invokeStartOrEndCallback(WsClient *wsClient, bool isStart);

private:
    static uint16_t getQeueDelay();

    bool _authenticated;
    AsyncWebSocketClient *_client;
    static ClientCallbackVector_t _clientCallback;

private:
    friend WsClientAsyncWebSocket;
    friend WebServerPlugin;

    using AsyncWebSocketVector = std::vector<AsyncWebSocket *>;

    static AsyncWebSocketVector _webSockets;
};

class WsClientAsyncWebSocket : public AsyncWebSocket {
public:
    WsClientAsyncWebSocket(const String &url, WsClientAsyncWebSocket **ptr = nullptr);
    virtual ~WsClientAsyncWebSocket();

    void shutdown();
    void disableSocket();
    void addWebSocketPtr(WsClientAsyncWebSocket **ptr);

private:
    void _enableAuthentication();

    WsClientAsyncWebSocket **_ptr;
};

#include <debug_helper_disable.h>
