/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Buffer.h"

class BufferStream : public Buffer, public Stream {
public:
    BufferStream();

    int available() override;
    size_t length() const;

    bool seek(long pos, int mode);
    size_t position() const;
    void clear();

    int read() override;
    int peek() override;

    char charAt(size_t pos) const;

    inline size_t write(uint8_t data) override {
        return Buffer::write(data);
    }
    inline size_t write(const char *str) {
        return Buffer::write(str, strlen(str));
    }
    inline size_t write(const uint8_t *data, size_t len) override {
        return Buffer::write((uint8_t *)data, len);
    }
    inline size_t write(const String &str) {
        return Buffer::write(str.c_str(), str.length());
    }

#if ESP32
    virtual void flush() {
    }
#endif

private:
    size_t _position;
};
