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
        else if (_position < _buffer.length()) {
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

    virtual size_t readBytes(char *buffer, size_t length) {
        return read((uint8_t *)buffer, length);
    }

    size_t read(uint8_t *buffer, size_t length);

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
    size_t _copy(uint8_t *buffer, size_t length);
    size_t _available();
    size_t _readBuffer(bool templateCheck = true);

private:
    bool isValidChar(char value) const {
        return isalnum(value) || value == '_' || value == '-' || value == '.';
    }

    constexpr static int maxTemplateNameSize = 64;

private:
    struct {
        typedef uint8_t *pointer;
        const SSIProxyStream &_p;
        pointer start() const {
            return position ? position : _p._buffer.begin();
        }
        ptrdiff_t to_offset(pointer ptr) const {
            return ptr - _p._buffer.begin();
        }
        pointer from_offset(ptrdiff_t offset) const {
            return _p._buffer.begin() + ((offset < 0) ? 0 : offset);
        }
        size_t template_length() const {
            return _p._length - start_length;
        }
        const char *template_name() const {
            return name.c_str();
        }
        bool in_buffer(pointer ptr) const {
            return ptr >= _p._buffer.begin() && ptr < _p._buffer.end();
        }
        size_t name_len() const {
            return marker != -1 ? (_p._buffer.end() - from_offset(marker)) : 0;
        }
        void eof() {
            marker = -1;
            position = _p._buffer.end();
        }
        ptrdiff_t marker;
        pointer position;
        size_t start_length;
        String name;
    } _template;

private:
    File &_file;
    Buffer _buffer;
    size_t _position;
    size_t _length;
    DataProviderInterface &_provider;
};
