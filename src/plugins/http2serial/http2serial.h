/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_HTTP2SERIAL
#define DEBUG_HTTP2SERIAL 0
#endif

#include <Arduino_compat.h>
#include <Buffer.h>
#include "ws_console_client.h"
#include "serial_handler.h"

class Http2Serial {
public:
    Http2Serial();
    virtual ~Http2Serial();

    void broadcastOutputBuffer();

    // inline void writeOutputBuffer(const String &buffer) {
    //     writeOutputBuffer(buffer.c_str(), buffer.length());
    // }
    // inline void writeOutputBuffer(const char *buffer, size_t len) {
    //     writeOutputBuffer((const uint8_t *)buffer, len);
    // }
    // void writeOutputBuffer(const uint8_t *buffer, size_t len);
    void writeOutputBuffer(SerialHandler::Client &client);

    bool isTimeToSend();
    void resetOutputBufferTimer();
    void clearOutputBuffer();

    static void outputLoop();
    static void onData(SerialHandler::Client &client);
    static Http2Serial *getInstance();
    static void createInstance();
    static void destroyInstance();

    size_t write(const uint8_t *buffer, int length);

public:
    static AsyncWebSocket *getConsoleServer();
    void setOutputBufferMaxSize(uint16_t size) {
        _outputBufferMaxSize = size;
    }
    void setOutputBufferDelay(uint16_t delay) {
        _outputBufferDelay = delay;
    }

private:
    void _outputLoop();

    static Http2Serial *_instance;

    bool _locked;
    bool _outputBufferEnabled;
    unsigned long _outputBufferFlushDelay;
    Buffer _outputBuffer;
    SerialHandler::Client &_client;

    uint16_t _outputBufferMaxSize;
    uint16_t _outputBufferDelay;
};
