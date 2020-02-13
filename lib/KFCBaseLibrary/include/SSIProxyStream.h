/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <Buffer.h>
#include <DataProviderInterface.h>

#ifndef DEBUG_SSI_PROXY_STREAM
#define DEBUG_SSI_PROXY_STREAM                    1
#endif

class SSIProxyStream : public Stream {
public:
    SSIProxyStream(File &file, DataProviderInterface &provider);
    ~SSIProxyStream();

    //File open() const;

    operator bool() const {
        return (bool)_file;
    }

    virtual int available() override {
        if (!_file) {
            return false;
        }
        else if (_pos < _buffer.length()) {
            return true;
        }
        return _file.available();
    }

    virtual int read() override {
        return _read();
    }

    virtual int peek() override {
        return _peek();
    }

    virtual size_t read(uint8_t *buffer, size_t length) {
        return _read(buffer, length);
    }

    size_t read(char *buffer, size_t length) {
        return _read((uint8_t *)buffer, length);
    }

    virtual size_t write(uint8_t) override {
        return 0;
    }

    virtual size_t write(const uint8_t *buffer, size_t size) override {
        return 0;
    }

    bool seek(uint32_t pos, SeekMode mode) {
        if (mode == SEEK_END) {
            pos = size() - pos;
        }
        else if (mode == SEEK_CUR) {
            pos = position() + (int)pos;
        }
        else if (mode == SEEK_SET) {
        }

        if (pos < position()) { // we cannot go backwards
            return false;
        }
        else {
            while (position() < pos && _read() != -1) {
                pos++;
            }
        }
        return position() == pos;
    }

    virtual size_t position() const {
        return _file.position();
    }

    virtual size_t size() const {
        return _file.size();
    }

    void close() {
        _file.close();
    }

#if defined(ESP32)
    virtual void flush() {}
#endif

private:
    int _read();
    int _peek();
    size_t _read(uint8_t *buffer, size_t length);
    int _available() const;
    int _readBuffer(bool templateCheck = true);

private:
    File &_file;
    Buffer _buffer;
    size_t _pos;
    size_t _length;
    struct {
        bool marker;
        uint8_t *pos;
        size_t _length;
        String name;
    } _template;
    DataProviderInterface &_provider;
};
