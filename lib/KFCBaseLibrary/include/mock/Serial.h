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

    size_t position() const override {
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


class FakeSerial : public Stdout {
public:
    FakeSerial() : Stdout() {
    }
    void begin(int baud) {
    }
    void end() {
    }
    virtual int available() {
        return 0;
    }
    virtual int read() {
        return -1;
    }
    virtual int peek() {
        return -1;
    }
    void flush(void) override {
        fflush(stdout);
    }
    virtual size_t write(uint8_t c) override {
        return write(&c, 1);
    }
    virtual size_t write(const uint8_t* buffer, size_t size) override {
        ::printf("%*.*s", size, size, buffer);
        _RPTN(_CRT_WARN, "%*.*s", size, size, buffer);
        return size;
    }
};

typedef FakeSerial HardwareSerial;

extern HardwareSerial Serial;
