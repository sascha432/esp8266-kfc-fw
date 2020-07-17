/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_SERIAL_HANDLER
#define DEBUG_SERIAL_HANDLER        0
#endif

#include <Arduino.h>
#include <functional>
#include <vector>
#include <Stream.h>
#include <StreamWrapper.h>
#include <NullStream.h>
#include <Buffer.h>
#include <EventScheduler.h>
#include <HardwareSerial.h>

class SerialWrapper : public Stream {
public:
    SerialWrapper();
    SerialWrapper(Stream &serial);
    virtual ~SerialWrapper() {
    }

    inline void setSerial(Stream &serial) {
        _serial = serial;
    }
    Stream &getSerial() const {
        return _serial;
    }

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *data, size_t len) override;
    virtual size_t write(const char *buffer);
#if defined(ESP32)
    virtual void flush();
#else
    virtual void flush() override;
#endif

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;

private:
    Stream &_serial;
};

class SerialHandler;

extern SerialHandler serialHandler;

class SerialHandler : public Stream {
public:
    typedef enum {
        RECEIVE     = 0x01,
        TRANSMIT    = 0x02,
        REMOTE_RX   = 0x04,
        LOCAL_TX    = 0x08,
        ANY         = 0xff
    } SerialDataType_t;

    typedef void (* SerialHandlerCallback_t)(uint8_t type, const uint8_t *buffer, size_t len);

    class Callback {
    public:
        Callback(SerialHandlerCallback_t callback, uint8_t flags) : _callback(callback), _flags(flags) {
        }
        inline bool operator ==(SerialHandlerCallback_t callback) const {
            return _callback == callback;
        }
        inline SerialHandlerCallback_t getCallback() const {
            return _callback;
        }
        inline uint8_t getFlags() const {
            return _flags;
        }
        inline bool hasType(SerialDataType_t type) const {
            return (_flags & type);
        }
        inline void invoke(uint8_t type, const uint8_t *buffer, size_t len) const {
            return _callback(type, buffer, len);
        }
    private:
        SerialHandlerCallback_t _callback;
        uint8_t _flags;
    };

    typedef std::vector<Callback> HandlersVector;

public:
    SerialHandler(StreamWrapper &wrapper);
    void clear();

    void begin();
    void end();

    void addHandler(SerialHandlerCallback_t callback, uint8_t flags);
    void removeHandler(SerialHandlerCallback_t callback);

    static void serialLoop();

    void writeToTransmit(SerialDataType_t type, const uint8_t *buffer, size_t len);
    void writeToTransmit(SerialDataType_t type, SerialHandlerCallback_t callback, const uint8_t *buffer, size_t len);
    void writeToReceive(SerialDataType_t type, const uint8_t *buffer, size_t len);
    void writeToReceive(SerialDataType_t type, SerialHandlerCallback_t callback, const uint8_t *buffer, size_t len);
    void receivedFromRemote(SerialHandlerCallback_t callback, const uint8_t *buffer, size_t len);

    // Stream &getSerial() const {
    //     return _wrapper.getSerial();
    // }
    // SerialWrapper &getWrapper() const {
    //     return _wrapper;
    // }
    static constexpr SerialHandler &getInstance() {
        return serialHandler;
    }

// Stream
public:
    virtual int available() override {
        return 0;
    }
    virtual int read() override {
        return -1;
    }
    virtual int peek() override {
        return -1;
    }

// Print
public:
    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual void flush() override {}

private:
    void _serialLoop();
    void _sendRxBuffer(SerialDataType_t type, SerialHandlerCallback_t callback, const uint8_t *buffer, size_t length);

private:
    HandlersVector _handlers;
    StreamWrapper &_wrapper;
};

extern NullStream NullSerial;
extern HardwareSerial Serial0;
extern Stream &Serial;
extern Stream &DebugSerial;
