/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "Stream.h"

class Stdout : public Stream {
public:
    Stdout() : Stream() {
        _size = 0xffff;
        _position = 0;
        _buffer = (uint8_t *)calloc(_size, 1);
    }
    ~Stdout() {
        close();
    }

    void close() {
        free(_buffer);
        _buffer = nullptr;
    }

    size_t position() const {
        return _position;
    }

    bool seek(long pos, int mode) {
        if (mode == SeekSet) {
            if (pos >= 0 && pos < _size) {
                _position = (uint16_t)pos;
                return true;
            }
        } else if (mode == SeekCur) {
            return seek(_position + pos, SeekSet);
        } else if (mode == SeekEnd) {
            return seek(_size - pos, SeekSet);
        }
        return false;
    }
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }

    int read() {
        uint8_t ch;
        if (read(&ch, sizeof(ch)) != 1) {
            return -1;
        }
        return ch;
    }
    int read(uint8_t *buffer, size_t len) {
        if (!_buffer) {
            return 0;
        }
        if (_position + len >= _size) {
            len = _size - _position;
        }
        memcpy(buffer, _buffer + _position, len);
        return len;
    }


    size_t write(char data) {
        return write((uint8_t)data);
    }
    size_t write(uint8_t data) {
        return write(&data, sizeof(data));
    }
    size_t write(const uint8_t *data, size_t len) override {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        ::printf("%04x-%04x: ", _position, _position + len - 1);
        for (size_t i = 0; i < len; i++) {
            ::printf("%02x ", data[i]);
        }
        for (size_t i = 0; i < len; i++) {
            if (isalnum(data[i])) {
                ::printf("%c", data[i]);
            } else {
                ::printf(".");
            }
        }
        ::printf("\n");
        if (!_buffer) {
            return len;
        }
        memcpy(_buffer + _position, data, len);
        _position += (uint16_t)len;
        return len;
    }

    void flush() {
    }

    size_t size() const {
        return _size;
    }

    int available() override {
        return _size - _position;
    }


private:
    uint8_t *_buffer;
    uint16_t _position;
    uint16_t _size;
};


class HardwareSerial : public Stdout {
public:
    HardwareSerial(const HardwareSerial &fake) = delete;

    HardwareSerial();
    ~HardwareSerial();

    void begin(int baud);
    void end();

    virtual int available();
    virtual int read();
    virtual int peek();
    virtual void flush(void) override;
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
};

using FakeSerial = HardwareSerial;

extern Stream &Serial;
extern HardwareSerial &Serial0;
