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
#include <StreamWrapper.h>
#include <Buffer.h>
#include <EventScheduler.h>

class SerialWrapper : public Stream {

public:
    SerialWrapper();
    SerialWrapper(Stream &serial);
    virtual ~SerialWrapper() {
    }

    inline void setSerial(Stream &serial) {
        _serial = serial;
    }
    inline Stream &getSerial() const {
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

    inline int __available() {
        return _serial.available();
    }
    inline int __read() {
        return _serial.read();
    }
    inline int __peek() {
        return _serial.peek();
    }

private:
    Stream &_serial;
};


class SerialHandler {
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

private:
    SerialHandler(SerialWrapper &wrapper);
    // virtual ~SerialHandler() {
    //     end();
    // }

public:
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

    inline Stream &getSerial() const {
        return _wrapper.getSerial();
    }
    inline SerialWrapper &getWrapper() const {
        return _wrapper;
    }
    inline static SerialHandler &getInstance() {
        return *SerialHandler::_instance;
    }

private:
    void _serialLoop();
    void _sendRxBuffer(SerialDataType_t type, SerialHandlerCallback_t callback, const uint8_t *buffer, size_t length);

private:
    HandlersVector _handlers;
    SerialWrapper &_wrapper;
    static SerialHandler *_instance;
};

// class NulStream : public Stream {
// public:
//     NulStream() { }
//     virtual int available() override { return 0; }
//     virtual int read() override { return -1; }
//     virtual int peek() override { return -1; }
//     virtual size_t read(uint8_t *buffer, size_t length) { return 0; }
//     size_t read(char *buffer, size_t length) { return 0; }
//     virtual size_t write(uint8_t) override { return 0; }
//     virtual size_t write(const uint8_t *buffer, size_t size) override { return 0; }
//     bool seek(uint32_t pos, SeekMode mode) { return false; }
// #if defined(ESP32)
//     virtual void flush() { }
// #else
//     virtual void flush() override { }
//     virtual size_t position() const { return 0; }
//     virtual size_t size() const { return 0; }
// #endif
// };

extern StreamWrapper MySerialWrapper;
extern Stream &MySerial;
extern Stream &DebugSerial;

