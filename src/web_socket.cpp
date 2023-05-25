/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <stl_ext/conv_utf8.h>
#include <PrintString.h>
#include <Buffer.h>
#include <DumpBinary.h>
#include "session.h"
#include "kfc_fw_config.h"
#include "logger.h"
#include "web_socket.h"
#include "../src/plugins/plugins.h"
#include  "spgm_auto_def.h"

#if DEBUG_WEB_SOCKETS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

 WsClient::ClientCallbackVector WsClient::_clientCallback;
 WsClient::AsyncWebSocketVector WsClient::_webSockets;
 SemaphoreMutex WsClient::_lock;

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

WsClientAsyncWebSocket::WsClientAsyncWebSocket(const String &url, WsClientAsyncWebSocket **ptr) :
    AsyncWebSocket(url),
    _ptr(ptr),
    _authenticatedClients(0)
{
    __LDBG_printf("this=%p url=%s ptr=%p *ptr=%p _ptr=%p *_ptr=%p", this, url.c_str(), ptr, ptr ? *ptr : nullptr, _ptr, _ptr ? *_ptr : nullptr);
    WsClient::_webSockets.push_back(this);
    if (_ptr) {
        if (*_ptr) {
            __DBG_panic("_instance already set ptr=%p this=%p url=%s", *_ptr, this, url.c_str());
        }
        *_ptr = this;
    }
    // the web socket is sharing the password with the web site
    // _enableAuthentication();
}

WsClientAsyncWebSocket::~WsClientAsyncWebSocket()
{
    __LDBG_printf("this=%p _ptr=%p *_ptr=%p clients=%u count=%u", this, _ptr, _ptr ? *_ptr : nullptr, getClients().length(), count());
    disableSocket();
    if (_ptr) {
        if (*_ptr != this) {
            __DBG_panic("_instance not set to this=%p ptr=%p", this, *_ptr);
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
    __LDBG_printf("count=%u authenticated=%u", WsClient::_webSockets.size(), _authenticatedClients);
    _authenticatedClients = 0;
    WsClient::_webSockets.erase(std::remove(WsClient::_webSockets.begin(), WsClient::_webSockets.end(), this), WsClient::_webSockets.end());
}

void WsClientAsyncWebSocket::addWebSocketPtr(WsClientAsyncWebSocket **ptr)
{
    __LDBG_printf("this=%p ptr=%u *ptr=%p _ptr=%p *_ptr=%p", this, ptr, ptr ? *ptr : nullptr, _ptr, _ptr ? *_ptr : nullptr);
    _ptr = ptr;
    *_ptr = this;
}

void WsClientAsyncWebSocket::_enableAuthentication()
{
    uint8_t buf[32];
    String password = String('\xff');
    ESP.random(buf, sizeof(buf));
    const char *ptr = (const char *)buf;
    for(uint8_t i = 0; i < (uint8_t)sizeof(buf); i++) {
        if (*ptr) {
            password += *ptr;
        }
        ptr++;
    }
    setAuthentication(String('\xff'), password);
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
        #else
            __DBG_printf("websocket %p has been removed, event type %u", server, type);
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
        // was acutally caused by reading the ADC too fast... analogRead(A0) every 150-200us
        // client->client()->setAckTimeout(30000); // added for bad WiFi connections

        wsClient->onConnect(data, len);

        for(const auto &callback: _clientCallback) {
            callback.first(ClientCallbackType::CONNECT, wsClient, server, callback.second);
        }

    }
    else if (type == WS_EVT_DISCONNECT) {

        Logger_notice(F(WS_PREFIX "Client disconnected"), WS_PREFIX_ARGS);
        wsClient->onDisconnect(data, len);

        WsClient::invokeStartOrEndCallback(wsClient, false);

        for(const auto &callback: _clientCallback) {
            callback.first(ClientCallbackType::DISCONNECT, wsClient, server, callback.second);
        }

        // WS_EVT_DISCONNECT is called in the destructor of AsyncWebSocketClient
        delete wsClient;
        client->_tempObject = nullptr;

    }
    else if (type == WS_EVT_ERROR) {

        __LDBG_printf("WS_EVT_ERROR wsClient %p", wsClient);

        auto str = printable_string(data, len, 16);
        str.trim();
        Logger_notice(F(WS_PREFIX "Error(%u): data=%s"), WS_PREFIX_ARGS, *reinterpret_cast<uint16_t *>(arg), str.c_str());
        wsClient->onError(WsClient::ERROR_FROM_SERVER, data, len);

    }
    else if (type == WS_EVT_PONG) {

        __LDBG_printf("WS_EVT_PONG wsClient %p", wsClient);
        wsClient->onPong(data, len);

    }
    else if (type == WS_EVT_DATA) {


        // #if DEBUG_WEB_SOCKETS
        //     wsClient->_displayData(wsClient, (AwsFrameInfo*)arg, data, len);
        // #endif

        constexpr size_t pingLen = constexpr_strlen("+iPING ");
        constexpr size_t sidLen = constexpr_strlen("+SID ");

        if (len > pingLen && strncmp_P((const char *)data, PSTR("+iPING "), pingLen) == 0) {
            char buffer[32];
            float response = NAN;
            size_t size = std::min(len - pingLen, sizeof(buffer) - 1);
            if (size) {
                memcpy(buffer, data + pingLen, size);
                buffer[size] = 0;
                response = atof(buffer);
            }
            client->text(PrintString(F("+iPONG %.3f"), response));
        }
        else if (wsClient->isAuthenticated()) {

            wsClient->onData(static_cast<AwsFrameType>(reinterpret_cast<AwsFrameInfo *>(arg)->opcode), data, len);

        }
        else if (len > 10 && strncmp_P((const char *)data, PSTR("+SID "), sidLen) == 0) {          // client sent authentication

            Buffer buffer = Buffer();
            auto dataPtr = reinterpret_cast<const char *>(data);
            dataPtr += sidLen;
            len -= sidLen;
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
                    callback.first(ClientCallbackType::AUTHENTICATED, wsClient, server, callback.second);
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
    }
    else if (type == WS_BINARY) {
        onBinary(data, len);
    }
    else {
        __LDBG_printf("type=%d", (int)type);
    }
}

void WsClient::invokeStartOrEndCallback(WsClient *wsClient, bool isStart)
{
    uint16_t authenticatedClients = 0;
    auto client = wsClient->getClient();
    WsClient::foreach(client->server(), nullptr, [&authenticatedClients](AsyncWebSocketClient *) {
        authenticatedClients++;
    });
    __DBG_assert_printf(*reinterpret_cast<WsClientAsyncWebSocket *>(client->server())->_ptr == client->server(), "WsClientAsyncWebSocket::_ptr does not match AsyncWebSocket");

    reinterpret_cast<WsClientAsyncWebSocket *>(client->server())->_authenticatedClients = authenticatedClients;

    __LDBG_printf("client=%p isStart=%u authenticatedClients=%u", wsClient, isStart, authenticatedClients);
    if (isStart) {
        if (authenticatedClients == 1) { // first client has been authenticated
            __LDBG_print("invoking onStart()");
            wsClient->onStart();
            for(const auto &callback: _clientCallback) {
                callback.first(ClientCallbackType::ON_START, wsClient, wsClient->getClient()->server(), callback.second);
            }
        }
    }
    else {
        if (authenticatedClients == 0) { // last client disconnected
            __LDBG_print("invoking onEnd()");
            wsClient->onEnd();
            for(const auto &callback: _clientCallback) {
                callback.first(ClientCallbackType::ON_END, wsClient, wsClient->getClient()->server(), callback.second);
            }
        }
    }
}

uint16_t WsClient::getQueueDelay()
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
    }
    else if (qSize > 4096) {
        qDelay = std::max(qDelay, (uint16_t)10);
    }
    else if (qSize > 1024) {
        qDelay = std::max(qDelay, (uint16_t)5);
    }
    return qDelay;
}

// validate server and sender
// server may be nullptr if sender is not nullptr
// sender may be nullptr for broadcasts
static bool __get_server(AsyncWebSocket *&server, WsClient *sender)
{
    if (!server) {
        if (!sender) {
            __DBG_printf("sender is nullptr");
            return false;
        }
        if (!sender->getClient()) {
            __DBG_printf("sender=%p get_client=%p", sender, sender->getClient());
            return false;
        }
        server = sender->getClient()->server();
    }
    return WsClient::hasAuthenticatedClients(server) && (server)->availableForWriteAll();
}

// validate server and client
// server may be nullptr
static bool __get_server(AsyncWebSocket *&server, AsyncWebSocketClient *client)
{
    if (!client) {
        __DBG_printf("client is nullptr");
        return false;
    }
    if (!server) {
        server = client->server();
    }
    return WsClient::hasAuthenticatedClients(server) && server->availableForWriteAll();
}

void WsClient::_broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer)
{
    #if ESP32
        esp_err_t err;
        if ((err = esp_task_wdt_add(nullptr)) != ESP_OK) {
            __DBG_printf_E("esp_task_wdt_add failed err=%x", err);
        }
    #endif
    auto qDelay = getQueueDelay();
    buffer->lock();
    WsClient::foreach(server, sender, [buffer, qDelay](AsyncWebSocketClient *client) {
        if (client->canSend()) {
            client->text(buffer);
            #if ESP32
                esp_task_wdt_reset();
            #elif ESP8266
                if (can_yield()) {
                    delay(qDelay); // let the device work on its tcp buffers
                }
            #endif
        }
    });
    buffer->unlock();
    server->_cleanBuffers();
    #if ESP32
        esp_task_wdt_delete(NULL);
    #endif
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, AsyncWebSocketMessageBuffer *buffer)
{
    if (__get_server(server, sender)) {
        _broadcast(server, sender, buffer);
    }
}

// AsyncWebSocketMessageBuffer *WsClient::jsonToBuffer(AsyncWebSocket *server, const JsonUnnamedObject &json)
// {
//     // more efficient than JsonBuffer
//     String str = json.toString();
//     size_t len = str.length();
//     auto cStr = str.__release();
//     if (!cStr) {
//         cStr = reinterpret_cast<char *>(calloc(1, len));
//     }
//     return server->makeBuffer(reinterpret_cast<uint8_t *>(cStr), len, false/* use cStr instead of allocating new memory and copying */);
//     // auto buffer = server->makeBuffer(str.length());
//     // if (buffer) {
//     //     memcpy((char *)buffer->get(), cStr, len + 1);
//     // }
//     // return buffer;
// }

AsyncWebSocketMessageBuffer *WsClient::utf8ToBuffer(AsyncWebSocket *server, const char *str, size_t length)
{
    size_t buflen = length;
    stdex::conv::utf8::strlen<stdex::conv::utf8::DefaultReplacement>(str, buflen, length);
    auto buffer = server->makeBuffer(buflen);
    if (buffer) {
        buffer->_len = buflen;
        stdex::conv::utf8::strcpy<stdex::conv::utf8::DefaultReplacement>(reinterpret_cast<char *>(buffer->get()), str, buflen, length);
        buffer->get()[buflen] = 0;
    }
    return buffer;
}

AsyncWebSocketMessageBuffer *WsClient::moveStringToBuffer(AsyncWebSocket *server, String &&str)
{
    size_t len = str.length();
    auto cStr = str.__release();
    if (!cStr) {
        cStr = reinterpret_cast<char *>(calloc(1, len));
    }
    return server->makeBuffer(reinterpret_cast<uint8_t *>(cStr), len, false/* use cStr instead of allocating new memory and copying */);
    // auto buffer = server->makeBuffer(str.length());
    // if (buffer) {
    //     memcpy((char *)buffer->get(), cStr, len + 1);
    // }
    // return buffer;
}

// void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const JsonUnnamedObject &json)
// {
//     if (!__get_server(server, sender)) {
//         return;
//     }
//     auto buffer = jsonToBuffer(server, json);
//     if (buffer) {
//         _broadcast(server, sender, buffer);
//     }
// }

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const uint8_t *str, size_t length)
{
    if (!__get_server(server, sender)) {
        return;
    }
    auto buffer = server->makeBuffer(length);
    if (buffer) {
        reinterpret_cast<uint8_t *>(memcpy_P(buffer->get(), str, length))[length] = 0;
        _broadcast(server, sender, buffer);
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const char *str, size_t length)
{
    if (!__get_server(server, sender)) {
        return;
    }
    auto buffer = utf8ToBuffer(server, str, length);
    if (buffer) {
        _broadcast(server, sender, buffer);
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const MQTT::Json::UnnamedObject &json)
{
    if (!__get_server(server, sender)) {
        return;
    }
    auto jsonStr = json.toString();
    broadcast(server, sender, std::move(jsonStr));
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, String &&str)
{
    if (!__get_server(server, sender)) {
        return;
    }
    auto buffer = moveStringToBuffer(server, std::move(str));
    if (buffer) {
        _broadcast(server, sender, buffer);
    }
}

void WsClient::broadcast(AsyncWebSocket *server, WsClient *sender, const __FlashStringHelper *str, size_t length)
{
    if (!__get_server(server, sender)) {
        return;
    }
    auto buffer = server->makeBuffer(length);
    if (buffer) {
        memcpy_P(buffer->get(), str, length);
        _broadcast(server, sender, buffer);
    }
}

void WsClient::safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const __FlashStringHelper *message)
{
    if (!__get_server(server, client)) {
        __LDBG_printf("no clients connected: server=%p client=%p message=%s", server, client, message);
        return;
    }
    #if ESP32
        if (esp_task_wdt_add(nullptr) != ESP_OK) {
            __DBG_printf_E("esp_task_wdt_add failed");
        }
    #endif
    WsClient::forsocket(server, client, [server, &message](AsyncWebSocketClient *socket) {
        if (socket->canSend()) {
            auto msg = String(message);
            auto buffer = utf8ToBuffer(server, msg.c_str(), msg.length()); //TODO change to __FlashStringHelper
            if (buffer) {
                socket->text(buffer);
                #if ESP32
                    esp_task_wdt_reset();
                #elif ESP8266
                    if (can_yield()) {
                        delay(WsClient::getQueueDelay());
                    }
                #endif
            }
        }
    });
    #if ESP32
        esp_task_wdt_delete(NULL);
    #endif
}

void WsClient::safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const String &message)
{
    if (!__get_server(server, client)) {
        __LDBG_printf("no clients connected: server=%p client=%p message=%s", server, client, message.c_str());
        return;
    }
    #if ESP32
        if (esp_task_wdt_add(nullptr) != ESP_OK) {
            __DBG_printf_E("esp_task_wdt_add failed");
        }
    #endif
    WsClient::forsocket(server, client, [server, &message](AsyncWebSocketClient *socket) {
        if (socket->canSend()) {
            auto buffer = utf8ToBuffer(server, message.c_str(), message.length());
            if (buffer) {
                socket->text(buffer);
                #if ESP32
                    esp_task_wdt_reset();
                #elif ESP8266
                    if (can_yield()) {
                        delay(WsClient::getQueueDelay());
                    }
                #endif
            }
        }
    });
    #if ESP32
        esp_task_wdt_delete(nullptr);
    #endif
}

void WsClient::safeSend(AsyncWebSocket *server, AsyncWebSocketClient *client, const MQTT::Json::UnnamedObject &json)
{
    if (!__get_server(server, client)) {
        __LDBG_printf("no clients connected: server=%p client=%p message=%s", server, client, json.toString().c_str());
        return;
    }
    #if ESP32
        if (esp_task_wdt_add(nullptr) != ESP_OK) {
            __DBG_printf_E("esp_task_wdt_add failed");
        }
    #endif
    WsClient::forsocket(server, client, [server, &json](AsyncWebSocketClient *socket) {
        if (socket->canSend()) {
            socket->text(json.toString());
            #if ESP32
                esp_task_wdt_reset();
            #elif ESP8266
                if (can_yield()) {
                    delay(WsClient::getQueueDelay());
                }
            #endif
        }
    });
    #if ESP32
        esp_task_wdt_delete(NULL);
    #endif
}
