/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

class NullStream : public Stream {
public:
    NullStream() {}

    virtual int available() override {
        return false;
    }
    virtual int read() override {
        return -1;
    }
    virtual int peek() override {
        return -1;
    }
    virtual size_t read(uint8_t *buffer, size_t length) {
        return 0;
    }

    virtual size_t write(uint8_t) override {
        return 0;
    }
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        return 0;
    }

    virtual size_t position() const {
        return 0;
    }
    virtual size_t size() const {
        return 0;
    }

#if defined(ESP32)
    virtual void flush() {}
#endif
};
