/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_WEB_SOCKETS
#   define DEBUG_WEB_SOCKETS (0 || defined(DEBUG_ALL))
#endif

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <ESPAsyncWebServer.h>
#if ESP8266
#include <interrupts.h>
#endif
#include "../src/plugins/mqtt/mqtt_strings.h"
#include "../src/plugins/mqtt/mqtt_json.h"

#if DEBUG_WEB_SOCKETS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define WS_PREFIX "ws[%s][%u] "
#define WS_PREFIX_ARGS server->url(), client->id()

class WsClient;
class WsClientAsyncWebSocket;
// class JsonUnnamedObject;

namespace WebServer {
    class Plugin;
}

//typedef std::function<WsClient *(WsClient *wsSClient, WsAwsEventType type, AsyncWebSocket *server, AsyncWebSocketClient *client, uint8_t *data, size_t len, void *arg)> WsEventHandlerCallback;
typedef std::function<WsClient *(AsyncWebSocketClient *client)> WsGetInstance;

class WsClient {
public:
    enum WsErrorType {
        ERROR_FROM_SERVER,
        ERROR_AUTHENTTICATION_FAILED,
    };

    enum class BinaryPacketType : uint16_t {
        RGB565_RLE_COMPRESSED_BITMAP,
        HLW8012_PLOT_DATA,
        TOUCHPAD_DATA,
        ADC_READINGS,
        LED_MATRIX_DATA,
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
    // no encoding, PROGMEM safe
    static void broadcast(AsyncWebSocket *server, WsClient *sender, const uint8_t *str, size_t length);
    // UTF-8 encoding
    static void broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer);
    static void broadcast(AsyncWebSocket *server, WsClient *sender, const char *str, size_t length);

    // no encoding
    static void broadcast(AsyncWebSocket *server, WsClient *sender, String &&str);
    static void broadcast(AsyncWebSocket *server, WsClient *sender, const __FlashStringHelper *str, size_t length);
    // static void broadcast(AsyncWebSocket *server, WsClient *sender, const JsonUnnamedObject &json);
    static void broadcast(AsyncWebSocket *server, WsClient *sender, const MQTT::Json::UnnamedObject &json);

    // validate server and client before sending
    static void safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const __FlashStringHelper *message);
    static void safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const String &message);
    static void safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const MQTT::Json::UnnamedObject &json);
    // static void safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const JsonUnnamedObject &json);

    // use hasAuthenticatedClients() instead
    static bool hasClients(AsyncWebSocket *server) {
        return hasAuthenticatedClients(server);
    }
    static bool hasAuthenticatedClients(AsyncWebSocket *server);


    // call function for each client that is connected, authenticated and is not sender
    // the clients sockets are passed to the function
    static void foreach(AsyncWebSocket *server, WsClient *sender, std::function<void(AsyncWebSocketClient *)> func) {
        InterruptLock lock;
        for(auto client: server->getClients()) {
            if (client->status() == WS_CONNECTED && client->_tempObject && client->_tempObject != sender && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                func(client);
            }
        }
    }

    // call function for "client" if connected and authenticated
    // the clients socket is passed to the function
    static void forclient(AsyncWebSocket *server, WsClient *forClient, std::function<void(AsyncWebSocketClient *)> func) {
        InterruptLock lock;
        for(auto client: server->getClients()) {
            if (client->status() == WS_CONNECTED && client->_tempObject && client->_tempObject == forClient && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                func(client);
                return;
            }
        }
    }

    // call function for "client" if connected and authenticated
    static void forsocket(AsyncWebSocket *server, AsyncWebSocketClient *socket, std::function<void(AsyncWebSocketClient *)> func) {
        InterruptLock lock;
        for(auto client: server->getClients()) {
            if (socket == client && client->status() == WS_CONNECTED && client->_tempObject && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                func(socket);
                return;
            }
        }
    }


public:
    enum class ClientCallbackType {
        CONNECT,
        AUTHENTICATED,
        DISCONNECT,
        ON_START,
        ON_END,
    };

    using ClientCallbackId = const void *;
    using ClientCallback = std::function<void(ClientCallbackType type, WsClient *client, AsyncWebSocket *server, ClientCallbackId id)>;
    using ClientCallbackPair = std::pair<ClientCallback, ClientCallbackId>;
    using ClientCallbackVector = std::vector<ClientCallbackPair>;

    static ClientCallbackVector::iterator findClientCallback(ClientCallbackId id) {
        return std::find_if(_clientCallback.begin(), _clientCallback.end(), [id](const ClientCallbackPair &pair) {
            return pair.second == id;
        });
    }

    // to remove a callback later, set id to a unique id
    static void addClientCallback(ClientCallback callback, ClientCallbackId id = nullptr) {
        if (id == nullptr || findClientCallback(id) == _clientCallback.end()) {
            _clientCallback.emplace_back(callback, id);
        }
    }
    static void removeClientCallback(ClientCallbackId id) {
        if (id) {
            _clientCallback.erase(std::remove_if(_clientCallback.begin(), _clientCallback.end(), [id](const ClientCallbackPair &pair) {
                return pair.second == id;
            }), _clientCallback.end());
        }
    }

protected:
    // does not validate server or sender
    static void _broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer);
    static void invokeStartOrEndCallback(WsClient *wsClient, bool isStart);

private:
    static uint16_t getQeueDelay();
    // static AsyncWebSocketMessageBuffer *jsonToBuffer(AsyncWebSocket *server, const JsonUnnamedObject &json);
    static AsyncWebSocketMessageBuffer *utf8ToBuffer(AsyncWebSocket *server, const char *str, size_t length);
    static AsyncWebSocketMessageBuffer *moveStringToBuffer(AsyncWebSocket *server, String &&str);

    bool _authenticated;
    AsyncWebSocketClient *_client;
    static ClientCallbackVector _clientCallback;

private:
    friend WsClientAsyncWebSocket;
    friend WebServer::Plugin;

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

    bool hasAuthenticatedClients() const {
        return _authenticatedClients;
    }

    uint16_t getAuthtenticatedClients() const {
        return _authenticatedClients;
    }

private:
    friend WsClient;

    void _enableAuthentication();

    WsClientAsyncWebSocket **_ptr;
    uint16_t _authenticatedClients;
};


inline bool WsClient::hasAuthenticatedClients(AsyncWebSocket *server)
{
    return server && reinterpret_cast<WsClientAsyncWebSocket *>(server)->hasAuthenticatedClients();
}


#include <debug_helper_disable.h>
