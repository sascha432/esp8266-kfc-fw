/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Buffer.h"

class BufferStream : public Buffer, public Stream {
public:
    //using Buffer::Buffer; // using seems to screw up intilizing the object with GCC. adding a constructor that calls the original works fine
    using Buffer::write;
    using Buffer::length;
    using Buffer::size;

    BufferStream() : Buffer(), _position(0) {
    }
    BufferStream(size_t size) : Buffer(size), _position(0) {
    }
    BufferStream(const String &str) : Buffer(str), _position(0) {
    }
    BufferStream(String &&str) : Buffer(str), _position(0) {
    }

    virtual int available() override;
    size_t available() const;

    bool seek(long pos, int mode);
    size_t position() const;
    void clear();
    void reset();

    virtual int read() override;
    virtual int peek() override;

    char charAt(size_t pos) const;

    virtual size_t write(uint8_t data) override {
        return Buffer::write(data);
    }
    virtual size_t write(const uint8_t *data, size_t len) override {
        return Buffer::write((uint8_t *)data, len);
    }

    virtual size_t readBytes(char *buffer, size_t length) {
        return readBytes((uint8_t *)buffer, length);
    }
    virtual size_t readBytes(uint8_t *buffer, size_t length);

#if ESP32
    virtual void flush() {
    }
#endif

private:
    size_t _position;
};
