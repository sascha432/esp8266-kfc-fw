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

public:
    static AsyncWebSocket *getConsoleServer();
    // find client by id that is connected and authenticated
    // client id is sent during authentication: +CLIENT_ID=0x12345678
    // nullptr will return first client
    static AsyncWebSocketClient *getClientById(const void *clientId);
    static AsyncWebSocketClient *getClientById(nullptr_t) {
        return getClientById(reinterpret_cast<void *>(0));
    }
    static AsyncWebSocketClient *getClientById(AsyncWebSocketClient *clientId) {
        return getClientById(reinterpret_cast<const void *>(clientId));
    }

private:
    void _outputLoop();

    static Http2Serial *_instance;

    bool _locked;
    bool _outputBufferEnabled;
    unsigned long _outputBufferFlushDelay;
    Buffer _outputBuffer;
    SerialHandler::Client &_client;

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
