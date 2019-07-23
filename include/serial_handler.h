/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

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

    void setSerial(Stream &serial) {
        _serial = serial;
    }
    Stream &getSerial() {
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

    virtual int available() override {
        return _serial.available();
    }
    virtual int read() override {
        return _serial.read();
    }
    virtual int peek() override {
        return _serial.peek();
    }


private:
    Stream &_serial;
};


class SerialHandler {
public:
    enum SerialDataType_t {
        RECEIVE     = 0x01,
        TRANSMIT    = 0x02,
        REMOTE_RX   = 0x04,
        LOCAL_TX    = 0x08,
    };
    typedef void (*SerialHandlerCb)(uint8_t type, const uint8_t *buffer, size_t len);

    struct SerialHandlers {
        SerialHandlerCb cb;
        uint8_t flags;
    };

    typedef std::vector<SerialHandlers> HandlersVector;

    SerialHandler(SerialWrapper &wrapper);
    // virtual ~SerialHandler() {
    //     end();
    // }

    void clear();

    void begin();
    void end();

    void addHandler(SerialHandlerCb callback, uint8_t flags);
    void removeHandler(SerialHandlerCb callback);

    void serialLoop();

    void writeToTransmit(SerialDataType_t type, const uint8_t *buffer, size_t len);
    void writeToTransmit(SerialDataType_t type, SerialHandlerCb callback, const uint8_t *buffer, size_t len);
    void writeToReceive(SerialDataType_t type, const uint8_t *buffer, size_t len);
    void writeToReceive(SerialDataType_t type, SerialHandlerCb callback, const uint8_t *buffer, size_t len);
    void receivedFromRemote(SerialHandlerCb callback, const uint8_t *buffer, size_t len);

    Stream &getSerial() {
        return _wrapper.getSerial();
    }
    SerialWrapper &getWrapper() {
        return _wrapper;
    }

private:
    void _sendRxBuffer(SerialDataType_t type, SerialHandlerCb callback, const uint8_t *buffer, size_t length);

private:
    HandlersVector _handlers;
    SerialWrapper &_wrapper;
};

class NulStream : public Stream {
public:
    NulStream() { }
    virtual int available() override { return 0; }
    virtual int read() override { return -1; }
    virtual int peek() override { return -1; }
    virtual size_t read(uint8_t *buffer, size_t length) { return 0; }
    size_t read(char *buffer, size_t length) { return 0; }
    virtual size_t write(uint8_t) override { return 0; }
    virtual size_t write(const uint8_t *buffer, size_t size) override { return 0; }
    bool seek(uint32_t pos, SeekMode mode) { return false; }
#if defined(ESP32)
    virtual void flush() { }
#else
    virtual void flush() override { }
    virtual size_t position() const { return 0; }
    virtual size_t size() const { return 0; }
#endif
};

extern StreamWrapper MySerialWrapper;
extern Stream &MySerial;
extern Stream &DebugSerial;
extern SerialHandler serialHandler;
