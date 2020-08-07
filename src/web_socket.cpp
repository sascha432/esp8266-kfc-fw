/**
 * Author: sascha_lammers@gmx.de
 */

//#include <sha512.h>
// #include <base64.hpp>
#include <Arduino_compat.h>
// #include <memory>
// #include <map>
// #include <vector>
// #include <algorithm>
// #include <functional>
#include <PrintString.h>
#include <Buffer.h>
#include <DumpBinary.h>
#include "session.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "web_socket.h"
#if HTTP2SERIAL_SUPPORT
#include "./plugins/http2serial/http2serial.h"
#endif

#if DEBUG_WEB_SOCKETS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

 WsClient::ClientCallbackVector_t WsClient::_clientCallback;
 WsClient::AsyncWebSocketVector WsClient::_webSockets;

extern bool generate_session_for_username(const String &username, String &password);

bool generate_session_for_username(const String &username, String &password)
{
    SessionHash::SaltBuffer salt;
    if (username.length() != sizeof(salt) * 2) {
        return false;
    }
    hex2bin(salt, sizeof(salt), username.c_str());
    password = generate_session_id(System::Device::getUsername(), System::Device::getPassword(), salt).c_str() + username.length();
    return true;
}

WsClientAsyncWebSocket::WsClientAsyncWebSocket(const String &url, WsClientAsyncWebSocket **ptr) : AsyncWebSocket(url), _ptr(ptr)
{
    // __LDBG_printf("WsClientAsyncWebSocket(): new=%p", this);
    WsClient::_webSockets.push_back(this);
    if (_ptr) {
        if (*_ptr) {
            __LDBG_printf("_instance already set %p", *_ptr);
        }
        *_ptr = this;
    }
    // the web socket is sharing the password with the web site
    // _enableAuthentication();
}

WsClientAsyncWebSocket::~WsClientAsyncWebSocket()
{
    // __LDBG_printf("~WsClientAsyncWebSocket(): delete=%p, clients=%u, connected=%u", this, getClients().length(), count());
    disableSocket();
    if (_ptr) {
        if (!*_ptr) {
            __LDBG_printf("_instance already set to %p", *_ptr);
        }
        *_ptr = nullptr;
    }
}

void WsClientAsyncWebSocket::shutdown()
{
    closeAll(503, FSPGM(Device_is_rebooting, "Device is rebooting...\n"));
    disableSocket();
}

void WsClientAsyncWebSocket::disableSocket()
{
    WsClient::_webSockets.erase(std::remove(WsClient::_webSockets.begin(), WsClient::_webSockets.end(), this), WsClient::_webSockets.end());
}

void WsClientAsyncWebSocket::addWebSocketPtr(WsClientAsyncWebSocket **ptr)
{
    _ptr = ptr;
    *_ptr = this;
}

void WsClientAsyncWebSocket::_enableAuthentication()
{
    uint8_t buf[32];
    String password = String('\xff');
    rng.rand(buf, sizeof(buf));
    const char *ptr = (const char *)buf;
    for(uint8_t i = 0; i < (uint8_t)sizeof(buf); i++) {
        if (*ptr) {
            password += *ptr;
        }
        ptr++;
    }
    setAuthentication(String('\xff'), password);
}


WsClient::WsClient(AsyncWebSocketClient *client) : _authenticated(false), _client(client)
{
}

WsClient::~WsClient()
{
}

void WsClient::setAuthenticated(bool authenticated)
{
    _authenticated = authenticated;
}

bool WsClient::isAuthenticated() const
{
    return _authenticated;
}

void WsClient::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, int type, uint8_t *data, size_t len, void *arg, WsGetInstance getInstance)
{
    if (WsClient::_webSockets.empty()) {
        return;
    }
    auto result = std::find(WsClient::_webSockets.begin(), WsClient::_webSockets.end(), server);
    if (result == WsClient::_webSockets.end()) {
#if DEBUG_WEB_SOCKETS
        __DBG_panic("websocket %p has been removed, event type %u", server, type);
#endif
        return;
    }

    WsClient *wsClient;
     if (client->_tempObject == nullptr) {
         // wsClient will be linked to AsyncWebSocketClient and deleted with WS_EVT_DISCONNECT when the socket gets destroyed
         client->_tempObject = wsClient = getInstance(client);
     }
     else {
         wsClient = reinterpret_cast<WsClient *>(client->_tempObject);
     }

    __LDBG_printf("event=%d wsClient=%p", type, wsClient);
    if (!wsClient) {
        __DBG_panic("getInstance() returned nullptr");
    }

    if (type == WS_EVT_CONNECT) {

        Logger_notice(F(WS_PREFIX "%s: Client connected"), WS_PREFIX_ARGS, client->remoteIP().toString().c_str());
        client->text(F("+REQ_AUTH"));
        client->keepAlivePeriod(60);
        client->client()->setAckTimeout(30000); // TODO added for bad WiFi connections

        wsClient->onConnect(data, len);

        for(const auto &callback: _clientCallback) {
            callback(ClientCallbackTypeEnum_t::CONNECT, wsClient);
        }

    } else if (type == WS_EVT_DISCONNECT) {

        Logger_notice(F(WS_PREFIX "Client disconnected"), WS_PREFIX_ARGS);
        wsClient->onDisconnect(data, len);

        WsClient::invokeStartOrEndCallback(wsClient, false);

        for(const auto &callback: _clientCallback) {
            callback(ClientCallbackTypeEnum_t::DISCONNECT, wsClient);
        }

        // WS_EVT_DISCONNECT is called in the destructor of AsyncWebSocketClient
        delete wsClient;
        client->_tempObject = nullptr;

    } else if (type == WS_EVT_ERROR) {

        __LDBG_printf("WS_EVT_ERROR wsClient %p", wsClient);

        auto str = printable_string(data, len, 128);
        str.trim();

        Logger_notice(F(WS_PREFIX "Error(%u): data=%s"), WS_PREFIX_ARGS, *reinterpret_cast<uint16_t *>(arg), str.c_str());
        wsClient->onError(WsClient::ERROR_FROM_SERVER, data, len);

    } else if (type == WS_EVT_PONG) {

        __LDBG_printf("WS_EVT_PONG wsClient %p", wsClient);
        wsClient->onPong(data, len);

    } else if (type == WS_EVT_DATA) {

        // #if DEBUG_WEB_SOCKETS
        //     wsClient->_displayData(wsClient, (AwsFrameInfo*)arg, data, len);
        // #endif

        if (wsClient->isAuthenticated()) {

            wsClient->onData(static_cast<AwsFrameType>(reinterpret_cast<AwsFrameInfo *>(arg)->opcode), data, len);

        } else if (len > 10 && strncmp_P((const char *)data, PSTR("+SID "), 5) == 0) {          // client sent authentication

            Buffer buffer = Buffer();
            auto dataPtr = reinterpret_cast<const char *>(data);
            dataPtr += 5;
            len -= 5;
            if (!buffer.reserve(len + 1)) {
                __LDBG_printf("allocation failed=%d", len + 1);
                return;
            }
            char *ptr = buffer.getChar();
            strncpy(ptr, dataPtr, len)[len] = 0;
            if (verify_session_id(ptr, System::Device::getName(), System::Device::getPassword())) {

                wsClient->setAuthenticated(true);
                WsClient::invokeStartOrEndCallback(wsClient, true);
                client->text(F("+AUTH_OK"));
                wsClient->onAuthenticated(data, len);

                for(const auto &callback: _clientCallback) {
                    callback(ClientCallbackTypeEnum_t::AUTHENTICATED, wsClient);
                }

            }
            else {
                wsClient->setAuthenticated(false);
                client->text(F("+AUTH_ERROR"));
                wsClient->onError(ERROR_AUTHENTTICATION_FAILED, data, len);
                client->close();
            }
        }
    }
}

/**
 *  gets called when the first client has been authenticated.
 **/
void WsClient::onStart()
{
    __LDBG_println();
}
/**
 * gets called once all authenticated clients have been disconnected. onStart() will be called
 * again when authenticated clients become available.
 **/
void WsClient::onEnd()
{
    __LDBG_println();
}

void WsClient::onData(AwsFrameType type, uint8_t *data, size_t len)
{
    if (type == WS_TEXT) {
        onText(data, len);
    } else if (type == WS_BINARY) {
        onBinary(data, len);
    } else {
        __LDBG_printf("type=%d", (int)type);
    }
}

void WsClient::addClientCallback(ClientCallback_t callback)
{
    _clientCallback.push_back(callback);
}

void WsClient::invokeStartOrEndCallback(WsClient *wsClient, bool isStart)
{
    uint16_t authenticatedClients = 0;
    auto client = wsClient->getClient();
    for(auto socket: client->server()->getClients()) {
        if (socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            authenticatedClients++;
        }
    }
    __LDBG_printf("client=%p isStart=%u authenticatedClients=%u", wsClient, isStart, authenticatedClients);
    if (isStart) {
        if (authenticatedClients == 1) { // first client has been authenticated
            __LDBG_print("invoking onStart()");
            wsClient->onStart();
        }
    }
    else {
        if (authenticatedClients == 0) { // last client disconnected
            __LDBG_print("invoking onEnd()");
            wsClient->onEnd();
        }
    }
}

uint16_t WsClient::getQeueDelay()
{
    // calculate delay depending on the queue size
    auto qCount = AsyncWebSocket::_getQueuedMessageCount();
    auto qSize = AsyncWebSocket::_getQueuedMessageSize();
    uint16_t qDelay = 1;
    if (qCount > 10) {
        qDelay = 50;
    }
    else if (qCount > 5) {
        qDelay = 10;
    }
    else if (qCount > 3) {
        qDelay = 5;
    }
    if (qSize > 8192) {
        qDelay = std::max(qDelay, (uint16_t)50);
    } else if (qSize > 4096) {
        qDelay = std::max(qDelay, (uint16_t)10);
    } else if (qSize > 1024) {
        qDelay = std::max(qDelay, (uint16_t)5);
    }
    return qDelay;
}

static bool __get_server(AsyncWebSocket **server,  WsClient *sender)
{
    if (*server == nullptr) {
        if (!sender) {
            __DBG_printf("sender=%p", sender);
            return false;
        }
        if (!sender->getClient()) {
            __DBG_printf("sender=%p getClient=%p", sender, sender->getClient());
            return false;
        }
        *server = sender->getClient()->server();
    }
    return true;
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer)
{
    if (__get_server(&server, sender)) {
        // __LDBG_printf("sender=%p, clients=%u, message=%s", sender, server->getClients().length(), buffer->get());
        auto qDelay = getQeueDelay();
        buffer->lock();
        for(auto socket: server->getClients()) {
            if (socket->status() == WS_CONNECTED && socket->_tempObject && socket->_tempObject != sender && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
                socket->text(buffer);
                delay(qDelay); // let the device work on its tcp buffers
            }
        }
        buffer->unlock();
        server->_cleanBuffers();
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const char *str, size_t length)
{
    if (__get_server(&server, sender)) {
        auto buffer = server->makeBuffer(length);
        memcpy(buffer->get(), str, length);
        broadcast(server, sender, buffer);
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const __FlashStringHelper *str, size_t length)
{
    if (__get_server(&server, sender)) {
        auto buffer = server->makeBuffer(length);
        memcpy_P(buffer->get(), str, length);
        broadcast(server, sender, buffer);
    }
}

void WsClient::safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const String &message)
{
    for(auto socket: server->getClients()) {
        if (client == socket && socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            __LDBG_printf("server=%p client=%p message=%s", server, client, message.c_str());
            client->text(message);
            delay(getQeueDelay());
            return;
        }
    }
    __LDBG_printf("server=%p client NOT found=%p message=%s", server, client, message.c_str());
}
