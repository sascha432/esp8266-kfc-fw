/**
 * Author: sascha_lammers@gmx.de
 */


#pragma once

#include "SSIProxyStream.h"

#if DEBUG_SSI_PROXY_STREAM
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

inline size_t SSIProxyStream::_available()
{
    if (_position >= _buffer.length()) {
        _readBuffer();
    }
    return _buffer.length() - _position;
}

inline int SSIProxyStream::read(uint8_t *buffer, size_t length)
{
    //__LDBG_printf("read_len=%d pos=%d length=%d size=%d", length, _position, _buffer.length(), _buffer.size());
    auto ptr = buffer;
    while (length && _available()) {
        auto copied = _copy(ptr, length);
        ptr += copied;
        length -= copied;
    }
    return ptr - buffer;
}

inline int SSIProxyStream::available()
{
    if (!_file) {
        return false;
    }
    else if (_position < _buffer.length()) {
        return true;
    }
    return _file.available();
}

inline int SSIProxyStream::read()
{
    auto data = peek();
    if (data != -1) {
        _position++;
        _template.position--;
    }
    return data;
}

inline int SSIProxyStream::peek()
{
    int data;
    if (_position >= _buffer.length()) {
        _readBuffer();
    }
    if (_position < _buffer.length()) {
        data = _buffer.get()[_position];
        return data;
    }
    else {
        close();
        return -1;
    }
}

#if DEBUG_SSI_PROXY_STREAM
#include "debug_helper_disable.h"
#endif
