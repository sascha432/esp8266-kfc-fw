/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <Buffer.h>
#include <DataProviderInterface.h>

#ifndef DEBUG_SSI_PROXY_STREAM
#   define DEBUG_SSI_PROXY_STREAM (0 || defined(DEBUG_ALL))
#endif

class SSIProxyStream : public Stream {
public:
    SSIProxyStream(File &file, DataProviderInterface &provider) :
        _template({ *this, 0, -1, nullptr, 0, String() }),
        _file(file),
        _position(0),
        _length(0),
        _provider(provider)
    {
        #if DEBUG_SSI_PROXY_STREAM
            _ramUsage = ESP.getFreeHeap();
        #endif
    }

    ~SSIProxyStream()
    {
        #if DEBUG_SSI_PROXY_STREAM
            __DBG_printf("ram=%u", _ramUsage);
        #endif
    }

    operator bool() const {
        return _file;
    }

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;

    virtual size_t readBytes(char *buffer, size_t length) {
        return read(reinterpret_cast<uint8_t *>(buffer), length);
    }

    int read(uint8_t *buffer, size_t length);

    virtual size_t write(uint8_t) override {
        return 0;
    }

    virtual size_t write(const uint8_t *buffer, size_t size) override {
        return 0;
    }

    bool seek(uint32_t pos, SeekMode mode);

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
    size_t _copy(uint8_t *buffer, size_t length);
    size_t _available();
    size_t _readBuffer(bool templateCheck = true);

private:
    inline bool isValidChar(char value) const {
        return isalnum(value) || value == '_' || value == '-' || value == '.';
    }

    constexpr static int maxTemplateNameSize = 64;

private:
    struct {
        using pointer = uint8_t *;

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

        char delim;
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
    #if DEBUG_SSI_PROXY_STREAM
        uint32_t _ramUsage;
    #endif
};

#include "SSIProxyStream.hpp"
