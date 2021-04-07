/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Buffer.h"
#include <PrintString.h>
#include  "spgm_auto_def.h"

#if DEBUG_BUFFER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

inline size_t Buffer::available() const
{
    if (_length < _size) {
        return _size - _length;
    }
    return 0;
}

inline void Buffer::setLength(size_t length)
{
    _length = length;
}

inline Buffer &Buffer::operator+=(const char *str)
{
    write(str, strlen(str));
    return *this;
}

inline Buffer &Buffer::operator+=(const String &str)
{
    write(str.c_str(), str.length());
    return *this;
}

inline Buffer &Buffer::operator+=(const __FlashStringHelper *str)
{
    auto pstr = reinterpret_cast<PGM_P>(str);
    write_P(pstr, strlen_P(pstr));
    return *this;
}

inline Buffer &Buffer::operator+=(const Buffer &buffer)
{
    write(buffer.get(), buffer.length());
    return *this;
}

inline void Buffer::remove(size_t index, size_t count)
{
    // // __LDBG_printf("index=%d count=%d", index, count);
    if (!count || index >= _length) {
        return;
    }
    if(count > _length - index) {
        count = _length - index;
    }
    _remove(index, count);
}

inline void Buffer::_remove(size_t index, size_t count)
{
    auto dst_begin = begin() + index;
    std::copy(dst_begin + count, end(), dst_begin);
    _length -= count;
#if BUFFER_ZERO_FILL
    std::fill(begin() + _length, _buffer_end(), 0);
#endif
}

inline String Buffer::toString() const
{
#if _WIN32 || _WIN64
    String tmp;
    tmp.reserve(_length);
    for (size_t i = 0; i < _length; i++) {
        tmp += (char)_buffer[i];
    }
    return tmp;
#elif 1
    return PrintString(_buffer, _length);
#else
    String tmp;
    if (tmp.reserve(_length)) {
        strncpy(tmp.begin(), (char *)_buffer, _length)[_length] = 0;
        tmp.setLen(_length);
}
    return tmp;
#endif
}

inline void Buffer::setBuffer(uint8_t *buffer, size_t size)
{
    clear();
    _buffer = buffer;
    _size = size;
}

inline size_t Buffer::write(const uint8_t *data, size_t len)
{
    // __LDBG_printf("len=%d", len);
    if (reserve(_length + len)) {
        memmove(_buffer + _length, data, len);
        _length += len;
        return len;
    }
    return 0;
}

inline size_t Buffer::write_P(PGM_P data, size_t len)
{
    // __LDBG_printf("len=%d", len);
    if (reserve(_length + len)) {
        memcpy_P(_buffer + _length, data, len);
        _length += len;
        return len;
    }
    return 0;
}

inline int Buffer::read()
{
    if (_length) {
        auto tmp = *_buffer;
        if (--_length) {
            memmove(_buffer, _buffer + 1, _length);
#if BUFFER_ZERO_FILL
            _buffer[_length] = 0;
#endif
        }
        else {
            clear();
        }
        return tmp;
    }
    return -1;
}

inline int Buffer::peek()
{
    if (_length) {
        return *_buffer;
    }
    return -1;
}

inline void Buffer::clear()
{
    // __LDBG_printf("len=%u size=%u ptr=%p", _length, _fp_size, _buffer);
    if (_buffer) {
        __LDBG_free(_buffer);
        _buffer = nullptr;
    }
    _size = 0;
    _length = 0;
}

inline Buffer &Buffer::operator=(const Buffer &buffer)
{
    _length = buffer._length;
    if (_changeBuffer(buffer._size)) {
        std::copy_n(buffer.begin(), _length, begin());
    }
    else {
        clear();
    }
    return *this;
}

inline Buffer &Buffer::operator=(String &&str)
{
    __LDBG_printf("len=%u size=%u ptr=%p", _length, _size, _buffer);
    this->~Buffer();
    MoveStringHelper::move(*this, std::move(str));
    return *this;
}

inline Buffer &Buffer::operator=(const String &str)
{
    __LDBG_printf("len=%u size=%u ptr=%p", _length, _size, _buffer);
    _length = 0;
    write(str);
    return *this;
}

inline bool Buffer::equals(const Buffer &buffer) const
{
    if (buffer._length != _length) {
        return false;
    }
    return memcmp(get(), buffer.get(), _length) == 0;
}

inline void Buffer::move(uint8_t **ptr)
{
    auto tmp = _buffer;
    _buffer = nullptr;
    _size = 0;
    _length = 0;
    *ptr = tmp;
    // remove from registered blocks
    __LDBG_NOP_free(tmp);
}

#if DEBUG_BUFFER
#include "debug_helper_disable.h"
#endif
