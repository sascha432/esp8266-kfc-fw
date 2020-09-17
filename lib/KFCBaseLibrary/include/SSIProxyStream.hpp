/**
 * Author: sascha_lammers@gmx.de
 */


#pragma once

#include "SSIProxyStream.h"

inline size_t SSIProxyStream::_available()
{
    if (_position >= _buffer.length()) {
        _readBuffer();
    }
    return _buffer.length() - _position;
}

inline size_t SSIProxyStream::read(uint8_t *buffer, size_t length)
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
