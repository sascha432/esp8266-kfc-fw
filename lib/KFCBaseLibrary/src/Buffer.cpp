/**
* Author: sascha_lammers@gmx.de
*/

#include <PrintString.h>
#include "debug_helper.h"
#include "Buffer.h"

#if 0
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

MoveStringHelper::MoveStringHelper()
{
}

void MoveStringHelper::move(Buffer &buf)
{
    __LDBG_println();
#if ESP8266
    if (length()) {
#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
        if (isSSO()) {
#else
        if (sso()) {
#endif
            buf.write(*this);
        }
        else {
            buf.setBuffer((uint8_t *)wbuffer(), capacity());
            buf.setLength(length());
            // register allocated block
            __LMDBG_IF(KFCMemoryDebugging::_alloc(DEBUG_HELPER_POSITION, capacity(), wbuffer()));
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

Buffer::Buffer() : _buffer(nullptr), _length(0), _size(0)
{
}

Buffer::Buffer(Buffer &&buffer) noexcept : _buffer(std::exchange(buffer._buffer, nullptr)), _length(std::exchange(buffer._length, 0)), _size(std::exchange(buffer._size, 0))
{
}

Buffer::Buffer(size_t size) : Buffer()
{
    _changeBuffer(size);
}

Buffer::~Buffer()
{
    __LDBG_printf("len=%u size=%u ptr=%p this=%p", _length, _size, _buffer, this);
    if (_buffer) {
        __LDBG_free(_buffer);
    }
    CHECK_MEMORY();
}

Buffer::Buffer(const __FlashStringHelper *str) : Buffer()
{
    write(str, strlen_P(reinterpret_cast<PGM_P>(str)));
}

Buffer::Buffer(String &&str) : Buffer()
{
    *this = std::move(str);
}

Buffer::Buffer(const String &str) : Buffer()
{
    write(str);
}

Buffer &Buffer::operator =(Buffer &&buffer) noexcept
{
    if (_buffer) {
        __LDBG_free(_buffer);
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
    __LDBG_printf("len=%u size=%u ptr=%p", _length, _size, _buffer);
    clear();
    static_cast<MoveStringHelper &&>(str).move(*this);
    return *this;
}

Buffer &Buffer::operator=(const String &str)
{
    __LDBG_printf("len=%u size=%u ptr=%p", _length, _size, _buffer);
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
    // __LDBG_printf("len=%u size=%u ptr=%p", _length, _size, _buffer);
    if (_buffer) {
        __LDBG_free(_buffer);
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
    // __LDBG_printf("len=%d", len);
    if (!reserve(_length + len)) {
        return 0;
    }
    memmove(_buffer + _length, data, len);
    _length += len;
    return len;
}

size_t Buffer::write_P(PGM_P data, size_t len)
{
    // __LDBG_printf("len=%d", len);
    if (!reserve(_length + len)) {
        return 0;
    }
    memcpy_P(_buffer + _length, data, len);
    _length += len;
    return len;
}

int Buffer::read()
{
    if (_length) {
        auto tmp = *_buffer;
        if (--_length) {
            memmove(_buffer, _buffer + 1, _length);
        }
        else {
            clear();
        }
        return tmp;
    }
    return -1;

}

int Buffer::peek()
{
    if (_length) {
        return *_buffer;
    }
    return -1;
}

void Buffer::remove(size_t index, size_t count)
{
    // __LDBG_printf("index=%d count=%d", index, count);
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
        // __LDBG_printf("size=%d", _size);
        if (_size == 0) {
            if (_buffer) {
                __LDBG_free(_buffer);
                _buffer = nullptr;
            }
        }
        else {
            _buffer = reinterpret_cast<uint8_t *>(_buffer ? __LDBG_realloc(_buffer, _size) : __LDBG_malloc(_size));
            if (_buffer == nullptr) {
                __LDBG_printf("alloc failed");
                _size = 0;
                _length = 0;
                return false;
            }
        }
    }
    if (_length > _size) {
        _length = _size;
    }
    // __LDBG_printf("length=%d size=%d", _length, _size);
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
    //if (nullptr == (_buffer = (uint8_t *)x_r_ealloc(_buffer, _size))) {
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
    return _changeBuffer(newSize);
}
