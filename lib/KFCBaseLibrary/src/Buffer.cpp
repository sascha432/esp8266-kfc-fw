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

#define CHECK_DATA_INT() \
    _debug_printf_P(PSTR("len=%u size=%u ptr=%p this=%p\n"), _length, _size, _buffer, this); \
    if (_length || _size || _buffer) { \
        __debugbreak_and_panic_printf_P(PSTR("len=%u size=%u ptr=%p this=%p\n"), _length, _size, _buffer, this); \
    }


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
    CHECK_DATA_INT();
    *this = std::move(buffer);
}

Buffer::Buffer(size_t size) : _buffer(), _length(), _size()
{
    CHECK_DATA_INT();
    _changeBuffer(size);
}

Buffer::~Buffer()
{
    _debug_printf("len=%u size=%u ptr=%p this=%p\n", _length, _size, _buffer, this);
    if (_buffer) {
        free(_buffer);
    }
    CHECK_MEMORY();
}

Buffer::Buffer(const __FlashStringHelper *str)
{
    CHECK_DATA_INT();
    write(str, strlen_P(reinterpret_cast<PGM_P>(str)));
}

Buffer::Buffer(String &&str)
{
    CHECK_DATA_INT();
    *this = std::move(str);
}

Buffer::Buffer(const String &str): Buffer()
{
    CHECK_DATA_INT();
    write(str);
}

Buffer &Buffer::operator =(Buffer &&buffer) noexcept
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    if (_buffer) {
        free(_buffer);
    }
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
    if (_changeBuffer(buffer._size)) {
        memcpy(_buffer, buffer._buffer, _length);
    }
    else {
        clear();
    }
    return *this;
}

Buffer &Buffer::operator=(String &&str)
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    clear();
    static_cast<MoveStringHelper &&>(str).move(*this);
    return *this;
}

Buffer &Buffer::operator=(const String &str)
{
    _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
    _length = 0;
    write(str);
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
    // _debug_printf("len=%u size=%u ptr=%p\n", _length, _size, _buffer);
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
    // _debug_printf_P(PSTR("len=%d\n"), len);
    if (!reserve(_length + len)) {
        return 0;
    }
    memmove(_buffer + _length, data, len);
    _length += len;
    return len;
}

size_t Buffer::write_P(PGM_P data, size_t len)
{
    // _debug_printf_P(PSTR("len=%d\n"), len);
    if (!reserve(_length + len)) {
        return 0;
    }
    memcpy_P(_buffer + _length, data, len);
    _length += len;
    return len;
}

void Buffer::remove(size_t index, size_t count)
{
    // _debug_printf_P(PSTR("index=%d count=%d\n"), index, count);
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

void Buffer::removeAndShrink(size_t index, size_t count, size_t minFree)
{
    remove(index, count);
    if (_length + minFree < _size) {
        shrink(_length);
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

bool Buffer::_changeBuffer(size_t newSize)
{
    if (_alignSize(newSize) != _size) {
        _size = _alignSize(newSize);
        // _debug_printf_P(PSTR("size=%d\n"), _size);
        if (_size == 0) {
            if (_buffer) {
                free(_buffer);
                _buffer = nullptr;
            }
        }
        else {
            if (_buffer == nullptr) {
                _buffer = (uint8_t *)malloc(_size);
            }
            else {
                _buffer = (uint8_t *)realloc(_buffer, _size);
            }
            if (_buffer == nullptr) {
                _debug_printf_P(PSTR("alloc failed\n"));
                _size = 0;
                _length = 0;
                return false;
            }
        }
    }
    if (_length > _size) {
        _length = _size;
    }
    // _debug_printf_P(PSTR("length=%d size=%d\n"), _length, _size);
    return true;
}

bool Buffer::reserve(size_t size)
{
    if (size > _size) {
        if (!_changeBuffer(size)) {
            return false;
        }
    }
    return true;
    //if (shrink) {
    //    if (size == 0) {
    //        clear();
    //        return true;
    //    }
    //}
    //else if (size < _size) {
    //    // shrink is false, ignore...
    //    return true;
    //}
    //_size = _alignSize(size);
    //if (nullptr == (_buffer = (uint8_t *)realloc(_buffer, _size))) {
    //    _size = 0;
    //    start_length = 0;
    //    return false;
    //}
    //if (start_length > _size) {
    //    start_length = _size;
    //}
    //return true;
}

bool Buffer::shrink(size_t newSize)
{
    if (newSize == 0) {
        newSize = _length;
    }
    if (_changeBuffer(newSize)) {
        return true;
    }
    clear();
    return false;
}
