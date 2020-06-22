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
#if HTTP2SERIAL
#include "./plugins/http2serial/http2serial.h"
#endif

#if DEBUG_WEB_SOCKETS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

 WsClient::ClientCallbackVector_t  WsClient::_clientCallback;
 WsClient::AsyncWebSocketVector WsClient::_webSockets;

 WsClient::WsClient(AsyncWebSocketClient * client) : _authenticated(false), _client(client)
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
    auto result = std::find(WsClient::_webSockets.begin(), WsClient::_webSockets.end(), server);
    if (result == WsClient::_webSockets.end()) {
        __debugbreak_and_panic_printf_P(PSTR("websocket %p has been removed, event type %u\n"), server, type);
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

    _debug_printf_P(PSTR("event=%d wsClient=%p\n"), type, wsClient);
    if (!wsClient) {
        __debugbreak_and_panic_printf_P(PSTR("getInstance() returned nullptr\n"));
    }

    if (type == WS_EVT_CONNECT) {
        client->ping();

        Logger_notice(F(WS_PREFIX "%s: Client connected"), WS_PREFIX_ARGS, client->remoteIP().toString().c_str());
        client->text(F("+REQ_AUTH"));

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

        _debug_printf_P(PSTR("WS_EVT_ERROR wsClient %p\n"), wsClient);

        PrintString str;
        DumpBinary dumper(str);
        dumper.dump(data, len);

        str.replace(String('\n'), String(' '));

        Logger_notice(F(WS_PREFIX "Error(%u): data='%s', length=%d"), WS_PREFIX_ARGS, *reinterpret_cast<uint16_t *>(arg), str.c_str(), len);
        wsClient->onError(WsClient::ERROR_FROM_SERVER, data, len);

    } else if (type == WS_EVT_PONG) {

        _debug_printf_P(PSTR("WS_EVT_PONG wsClient %p\n"), wsClient);
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
                _debug_printf_P(PSTR("allocation failed=%d\n"), len + 1);
                return;
            }
            char *ptr = buffer.getChar();
            strncpy(ptr, dataPtr, len)[len] = 0;
            if (verify_session_id(ptr, config.getDeviceName(), config._H_STR(Config().device_pass))) {

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
    _debug_println();
}
/**
 * gets called once all authenticated clients have been disconnected. onStart() will be called
 * again when authenticated clients become available.
 **/
void WsClient::onEnd()
{
    _debug_println();
}

void WsClient::onData(AwsFrameType type, uint8_t *data, size_t len)
{
    if (type == WS_TEXT) {
        onText(data, len);
    } else if (type == WS_BINARY) {
        onBinary(data, len);
    } else {
        _debug_printf_P(PSTR("type=%d\n"), (int)type);
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
    _debug_printf_P(PSTR("client=%p isStart=%u authenticatedClients=%u\n"), wsClient, isStart, authenticatedClients);
    if (isStart) {
        if (authenticatedClients == 1) { // first client has been authenticated
            _debug_println(F("invoking onStart()"));
            wsClient->onStart();
        }
    }
    else {
        if (authenticatedClients == 0) { // last client disconnected
            _debug_println(F("invoking onEnd()"));
            wsClient->onEnd();
        }
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer)
{
    if (server == nullptr) {
        server = sender->getClient()->server();
    }
    // _debug_printf_P(PSTR("sender=%p, clients=%u, message=%s\n"), sender, server->getClients().length(), buffer->get());
    buffer->lock();
    for(auto socket: server->getClients()) {
        if (socket->status() == WS_CONNECTED && socket->_tempObject && socket->_tempObject != sender && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            socket->text(buffer);
        }
    }
    buffer->unlock();
    server->_cleanBuffers();
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const char *str, size_t length)
{
    if (server == nullptr) {
        server = sender->getClient()->server();
    }
    auto buffer = server->makeBuffer(length);
    memcpy(buffer->get(), str, length);
    broadcast(server, sender, buffer);
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const __FlashStringHelper *str, size_t length)
{
    if (server == nullptr) {
        server = sender->getClient()->server();
    }
    auto buffer = server->makeBuffer(length);
    memcpy_P(buffer->get(), str, length);
    broadcast(server, sender, buffer);
}

void WsClient::safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const String &message)
{
    for(auto socket: server->getClients()) {
        if (client == socket && socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
            _debug_printf_P(PSTR("server=%p client=%p message=%s\n"), server, client, message.c_str());
            client->text(message);
            return;
        }
    }
    _debug_printf_P(PSTR("server=%p client NOT found=%p message=%s\n"), server, client, message.c_str());
}
