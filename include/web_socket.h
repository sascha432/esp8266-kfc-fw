/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <ESPAsyncWebServer.h>

#define WS_PREFIX "ws[%s][%u] "
#define WS_PREFIX_ARGS server->url(), client->id()

#define DEBUG_WEB_SOCKETS 0

#if DEBUG_WEB_SOCKETS
#include "debug_local_def.h"
#else
#include "debug_local_undef.h"
#endif

class WsClient;

//typedef std::function<WsClient *(WsClient *wsSClient, WsAwsEventType type, AsyncWebSocket *server, AsyncWebSocketClient *client, uint8_t *data, size_t len, void *arg)> WsEventHandlerCallback;
typedef std::function<WsClient *(AsyncWebSocketClient *client)> WsGetInstance;

struct WsClientPair {
    WsClient *wsClient;
    AsyncWebSocketClient *socket;
};

typedef std::vector<WsClientPair> WsClientManagerVector;
typedef std::vector<WsClient *> WsAuthClientsVector;

class WsClientManager {
public:
    WsClientManager();
    virtual ~WsClientManager();

    void add(WsClient *wsClient, AsyncWebSocketClient *socket);
    void remove(WsClient *wsClient);
    void remove(AsyncWebSocketClient *wsClient);

    // send message if socket is valid
    bool safeSend(AsyncWebSocketClient *socket, const String &message);
protected:
    WsClient *getClient(AsyncWebSocketClient *socket);
    int getClientCount(bool isAuthenticated = false);
public:
    WsClientManagerVector &getClients();

    void setClientAuthenticated(WsClient *wsClient, bool isAuthenticated);

    static WsClientManager *getWsClientManager();
    static void removeWsClient(WsClient *wsClient);
    static int getWsClientCount(bool isAuthenticated);
    static WsClient *getWsClient(AsyncWebSocketClient *socket);


private:
#if DEBUG_WEB_SOCKETS
    void _displayStats();
#endif

private:
    static WsClientManager *wsClientManager;

    WsClientManagerVector _list;
    WsAuthClientsVector _authenticated;
};

class WsClient {
public:
    enum WsErrorType {
        ERROR_FROM_SERVER,
        ERROR_AUTHENTTICATION_FAILED,
    };

    WsClient(AsyncWebSocketClient *client);
    virtual ~WsClient();

    void setAuthenticated(bool authenticated);
    bool isAuthenticated();

#if WEB_SOCKET_ENCRYPTION
    bool isEncrypted();

    void initEncryption(uint8_t *iv, uint8_t *salt);
#endif

    void setClient(AsyncWebSocketClient *client);
    AsyncWebSocketClient *getClient();

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


    // static WsClient *getInstance(AsyncWebSocketClient *socket);

#if DEBUG_WEB_SOCKETS
public:
    void _displayData(WsClient *wsClient, AwsFrameInfo *info, uint8_t *data, size_t len);
#endif

private:
    bool _authenticated;
    bool _isEncryped;
#if WEB_SOCKET_ENCRYPTION
    uint8_t *_iv;
    uint8_t *_salt;
#endif
    AsyncWebSocketClient *_client;
};
