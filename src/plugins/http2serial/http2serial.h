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

    inline void writeOutputBuffer(const String &buffer) {
        writeOutputBuffer(buffer.c_str(), buffer.length());
    }
    inline void writeOutputBuffer(const char *buffer, size_t len) {
        writeOutputBuffer((const uint8_t *)buffer, len);
    }
    void writeOutputBuffer(const uint8_t *buffer, size_t len);

    bool isTimeToSend();
    void resetOutputBufferTimer();
    void clearOutputBuffer();

    static void outputLoop();
    static void onData(uint8_t type, const uint8_t *buffer, size_t len);
    static Http2Serial *getInstance();
    static void createInstance();
    static void destroyInstance();

    SerialHandler *getSerialHandler() const;

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
    SerialHandler *_serialHandler;

    uint16_t _outputBufferMaxSize;
    uint16_t _outputBufferDelay;
};
