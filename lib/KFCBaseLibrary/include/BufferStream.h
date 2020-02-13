/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Buffer.h"

class BufferStream : public Buffer, public Stream {
public:
    using Buffer::Buffer;
    using Buffer::write;
    using Buffer::length;
    using Buffer::size;

    BufferStream();

    virtual int available() override;
    size_t available() const;

    bool seek(long pos, int mode);
    size_t position() const;
    void clear();
    void reset();
    //size_t size() const {
    //    return Buffer::size();
    //}
    //size_t length() const {
    //    return Buffer::length();
    //}

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
        return _read((uint8_t *)buffer, length);
    }

    virtual size_t readBytes(uint8_t *buffer, size_t length) {
        return _read(buffer, length);
    }

    //BufferStream &operator+=(const char *str) {
    //    static_cast<Buffer &>(*this) += str;
    //}
    //BufferStream &operator+=(const String &str) {
    //    static_cast<Buffer &>(*this) += str;
    //}
    //BufferStream &operator+=(const __FlashStringHelper *str) {
    //    static_cast<Buffer &>(*this) += str;
    //}
    //BufferStream &operator+=(const Buffer &buffer) {
    //    static_cast<Buffer &>(*this) += buffer;
    //}

#if ESP32
    virtual void flush() {
    }
#endif

private:
    size_t _read(uint8_t *buffer, size_t length);

private:
    size_t _position;
};
