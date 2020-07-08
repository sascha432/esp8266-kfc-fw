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
#include "../src/generated/FlashStringGeneratorAuto.h"

#define WS_PREFIX "ws[%s][%u] "
#define WS_PREFIX_ARGS server->url(), client->id()

class WsClient;

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
    bool _authenticated;
    AsyncWebSocketClient *_client;
    static ClientCallbackVector_t _clientCallback;

// private:
public:
    friend class WsClientAsyncWebSocket;

    typedef std::vector<AsyncWebSocket *> AsyncWebSocketVector;

    static AsyncWebSocketVector _webSockets;
};


class WsClientAsyncWebSocket : public AsyncWebSocket {
public:
    WsClientAsyncWebSocket(const String &url) : AsyncWebSocket(url) {
        // debug_printf("WsClientAsyncWebSocket(): new=%p\n", this);
        WsClient::_webSockets.push_back(this);
    }
    ~WsClientAsyncWebSocket() {
        // debug_printf("~WsClientAsyncWebSocket(): delete=%p, clients=%u, connected=%u\n", this, getClients().length(), count());
        disableSocket();
    }

    void shutdown() {
        closeAll(503, String(FSPGM(Device_is_rebooting, "Device is rebooting...\n")).c_str());
        disableSocket();
    }

    void disableSocket() {
        WsClient::_webSockets.erase(std::remove(WsClient::_webSockets.begin(), WsClient::_webSockets.end(), this), WsClient::_webSockets.end());
    }
};
