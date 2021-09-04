/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_HTTP2SERIAL
#define DEBUG_HTTP2SERIAL 0
#endif

#include <Arduino_compat.h>
#include <Buffer.h>
#include <kfc_fw_config.h>
#include "ws_console_client.h"
#include "serial_handler.h"

#if DEBUG_HTTP2SERIAL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

class Http2Serial {
public:
    Http2Serial();
    virtual ~Http2Serial();

    void broadcastOutputBuffer();
    void writeOutputBuffer(Stream &client);

    bool isTimeToSend();
    void resetOutputBufferTimer();
    void clearOutputBuffer();

    static void outputLoop();
    static void onData(Stream &client);
    static Http2Serial *getInstance();
    static void createInstance();
    static void destroyInstance();

    size_t write(const uint8_t *buffer, int length);
    void lockClient(bool lock);

public:
    static bool hasAuthenticatedClients();
    static WsClientAsyncWebSocket *getServerSocket();
    // find client by id that is connected and authenticated
    // client id is sent during authentication: +CLIENT_ID=0x12345678
    // nullptr will return first client
    static AsyncWebSocketClient *getClientById(const void *clientId);
    static AsyncWebSocketClient *getClientById(nullptr_t);
    static AsyncWebSocketClient *getClientById(AsyncWebSocketClient *clientId);

    static void eventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

private:
    void _outputLoop();

    static Http2Serial *_instance;

    SerialHandler::Client &_client;
    uint32_t _outputBufferFlushDelay;
    Buffer _outputBuffer;
    bool _locked;
    bool _outputBufferEnabled;
    bool _clientLocked;

    // The serial buffer delay is to increase the size of the web socket packets. The delay allows to
    // collect more data in the buffer before it is sent to the client. There is a certain threshold
    // where the small packets cause more lagg than having a delay
    //
    // (buffer size < kOutputBufferMinSize) OR (kOutputBufferFlushDelay not over) = wait
    // buffer size >= kOutputBufferMaxSize = send immediately


    // delay before sending the first message
    static constexpr size_t kOutputBufferFlushDelay = HTTP2SERIAL_SERIAL_BUFFER_FLUSH_DELAY;
    // max. size of the output buffer before sending a message without delay
    static constexpr size_t kOutputBufferMaxSize = HTTP2SERIAL_SERIAL_BUFFER_MAX_LEN;
    // min. size before sending a message and the delay is not over
    static constexpr size_t kOutputBufferMinSize = HTTP2SERIAL_SERIAL_BUFFER_MIN_LEN;
#if defined(ESP8266)
    static constexpr size_t kMaxTcpBufferSize = TCP_SND_BUF;
#else
    static constexpr size_t kMaxTcpBufferSize = 1024;
#endif

    static_assert(kOutputBufferMaxSize < (kMaxTcpBufferSize - 128), "might not fit into the TCP buffers of the device");
};

extern WsClientAsyncWebSocket *wsSerialConsole;

inline bool Http2Serial::isTimeToSend()
{
    return (_outputBuffer.length() != 0) && (kOutputBufferFlushDelay == 0 || (millis() > _outputBufferFlushDelay));
}

inline void Http2Serial::resetOutputBufferTimer()
{
    _outputBufferFlushDelay = millis() + kOutputBufferFlushDelay;
}

inline void Http2Serial::clearOutputBuffer()
{
    _outputBuffer.clear();
    resetOutputBufferTimer();
}

inline void Http2Serial::_outputLoop()
{
    if (kOutputBufferFlushDelay == 0 || isTimeToSend()) {
        broadcastOutputBuffer();
    }
    // auto handler = getSerialHandler();
    // if (handler != &SerialHandler::getInstance()) {
    //     handler->serialLoop();
    // }
}

inline void Http2Serial::outputLoop()
{
    __DBG_validatePointerCheck(_instance, VP_HSU);
    _instance->_outputLoop();
}

inline Http2Serial *Http2Serial::getInstance()
{
    return _instance;
}

inline void Http2Serial::createInstance()
{
    destroyInstance();
    if (!_instance) {
        _instance = new Http2Serial();
    }
    __LDBG_printf("inst=%p", _instance);
}

inline void Http2Serial::destroyInstance()
{
    __LDBG_printf("inst=%p", _instance);
    __DBG_validatePointerCheck(_instance, VP_NHS);
    if (_instance) {
        __DBG_validatePointerCheck(_instance, VP_HSU);
        delete _instance;
        _instance = nullptr;
    }
}

inline size_t Http2Serial::write(const uint8_t *buffer, int length)
{
    if (_clientLocked) {
        return length;
    }
    return _client.write(buffer, length);
}

inline void Http2Serial::lockClient(bool lock)
{
    _clientLocked = lock;
}

inline WsClientAsyncWebSocket *Http2Serial::getServerSocket()
{
    return wsSerialConsole;
}

inline bool Http2Serial::hasAuthenticatedClients()
{
    return wsSerialConsole && wsSerialConsole->hasAuthenticatedClients();
}

inline void WsConsoleClient::onStart()
{
    __LDBG_printf("first client has been authenticated, Http2Serial instance %p", Http2Serial::getInstance());
    Http2Serial::createInstance();
}

inline void WsConsoleClient::onEnd()
{
    __LDBG_printf("no authenticated clients connected, Http2Serial instance %p", Http2Serial::getInstance());
    Http2Serial::destroyInstance();
}

inline AsyncWebSocketClient *Http2Serial::getClientById(nullptr_t)
{
    return getClientById(reinterpret_cast<void *>(0));
}

inline AsyncWebSocketClient *Http2Serial::getClientById(AsyncWebSocketClient *clientId)
{
    return getClientById(reinterpret_cast<const void *>(clientId));
}

inline void Http2Serial::eventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WsClient::onWsEvent(server, client, type, data, len, arg, WsConsoleClient::getInstance);
}

#if DEBUG_HTTP2SERIAL
#include <debug_helper_disable.h>
#endif
