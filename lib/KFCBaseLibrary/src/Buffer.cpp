/**
* Author: sascha_lammers@gmx.de
*/

#include <PrintString.h>
#include "Buffer.h"

#if 0
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif


MoveStringHelper::MoveStringHelper() {
}

void MoveStringHelper::move(Buffer &buf) {
    _debug_println("");
#if ESP8266
    if (length()) {
        if (sso()) {
    debug_println("");
            buf.write(*this);
        }
        else {
    debug_println("");
            buf.setBuffer((uint8_t *)wbuffer(), capacity());
            buf.setLength(length());
        }
        setSSO(false);
        setCapacity(0);
        setLen(0);
        setBuffer(nullptr);
    }
#else
    buf.write(*this);
    *this = MoveStringHelper();
#endif
}

Buffer::Buffer(Buffer &&buffer) noexcept
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    *this = std::move(buffer);
}

Buffer::Buffer(size_t size) : _buffer(nullptr), _length(0), _size(size)
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    if (_size) {
        _size = _alignSize(_size);
        if (nullptr == (_buffer = (uint8_t *)malloc(_size))) {
            _size = 0;
        }
    }
}

Buffer::Buffer() : _buffer(nullptr), _length(0), _size(0)
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
}

Buffer::~Buffer()
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    if (_buffer) {
        free(_buffer);
    }
}

Buffer::Buffer(const __FlashStringHelper *str)
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    write(str, strlen_P(reinterpret_cast<PGM_P>(str)));
}

Buffer::Buffer(const String &str)
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    new(this)Buffer();
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    write(str);
}

Buffer::Buffer(String &&str)
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    _buffer = nullptr;
    _length = 0;
    _size = 0;
    static_cast<MoveStringHelper &&>(str).move(*this);
}

Buffer &Buffer::operator =(Buffer &&buffer) noexcept
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    _buffer = buffer._buffer;
    _length = buffer._length;
    _size = buffer._size;
    buffer._buffer = nullptr;
    buffer._length = 0;
    buffer._size = 0;
    return *this;
}

Buffer &Buffer::operator =(const Buffer &buffer)
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    _length = buffer._length;
    if (reserve(buffer._size, true)) {
        // reserve limits _length if shrink is set to true
        memcpy(_buffer, buffer._buffer, _length);
    }
    return *this;
}

bool Buffer::equals(const Buffer &buffer) const
{
    if (buffer._length != _length) {
        return false;
    }
    return memcmp(get(), buffer.get(), _length) == 0;
}

void Buffer::clear()
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
    _size = 0;
    _length = 0;
}

void Buffer::move(uint8_t **ptr)
{
    auto tmp = _buffer;
    _buffer = nullptr;
    _size = 0;
    _length = 0;
    *ptr = tmp;
}

char *Buffer::getNulByteString()
{
    _length -= write(0);
    return getChar();
}

void Buffer::setBuffer(uint8_t *buffer, size_t size)
{
    clear();
    _buffer = buffer;
    _size = size;
}

size_t Buffer::size() const
{
    return _size;
}

size_t Buffer::length() const
{
    return _length;
}

bool Buffer::reserve(size_t size, bool shrink)
{
    if (shrink) {
        if (size == 0) {
            clear();
            return true;
        }
    }
    else if (size < _size) {
        // shrink is false, ignore...
        return true;
    }
    _size = _alignSize(size);
    if (nullptr == (_buffer = (uint8_t *)realloc(_buffer, _size))) {
        _size = 0;
        _length = 0;
        return false;
    }
    if (_length > _size) {
        _length = _size;
    }
    return true;
}

// bool Buffer::available(size_t len) const
// {
//     return (len + _length <= _size);
// }

size_t Buffer::available() const
{
    if (_length < _size) {
        return _size - _length;
    }
    return 0;
}

String Buffer::toString() const
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

size_t Buffer::write(const uint8_t *data, size_t len)
{
    if (_length + len > _size) {
        if (!reserve(_length + len)) {
            return 0;
        }
    }
    memmove(_buffer + _length, data, len);
    _length += len;
    return len;
}

size_t Buffer::write_P(PGM_P data, size_t len)
{
    if (_length + len > _size) {
        if (!reserve(_length + len)) {
            return 0;
        }
    }
    memcpy_P(_buffer + _length, data, len);
    _length += len;
    return len;
}

void Buffer::remove(unsigned int index, size_t count)
{
    if(index >= _length) {
        return;
    }
    if(count <= 0) {
        return;
    }
    if(count > _length - index) {
        count = _length - index;
    }
    _length = _length - count;
    if (_length - index) {
        memmove(_buffer + index, _buffer + index + count, _length - index);
    }
}

void Buffer::removeAndShrink(unsigned int index, size_t count, size_t minFree)
{
    remove(index, count);
    if (_length < _size + minFree) {
        reserve(_length, true);
    }
}

Buffer& Buffer::operator+=(const char *str)
{
    write((uint8_t *)str, strlen(str));
    return *this;
}

Buffer& Buffer::operator+=(const String &str)
{
    write((uint8_t *)str.c_str(), str.length());
    return *this;
}

Buffer &Buffer::operator+=(const __FlashStringHelper *str)
{
    auto pstr = reinterpret_cast<PGM_P>(str);
    write_P(pstr, strlen_P(pstr));
    return *this;
}

Buffer &Buffer::operator+=(const Buffer &buffer)
{
    write(buffer.get(), buffer.length());
    return *this;
}
