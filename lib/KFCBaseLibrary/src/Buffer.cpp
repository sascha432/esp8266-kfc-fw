/**
* Author: sascha_lammers@gmx.de
*/

#include <PrintString.h>
#include "debug_helper.h"
#include "Buffer.h"

#if DEBUG_BUFFER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

MoveStringHelper::MoveStringHelper()
{
}

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
#define WSTRING_IS_SSO(s)               s.isSSO()
#else
#define WSTRING_IS_SSO(s)               s.sso()
#endif

char *MoveStringHelper::move(String &&_str, int *allocSize)
{
    auto &&str = static_cast<MoveStringHelper &&>(_str);
    char *buf;
    int _allocSize;
    __LDBG_println();
    auto len = str.length();
#if ESP8266
    if (len) {
        if (WSTRING_IS_SSO(str)) {
            _allocSize = (len + (1 << 3)) & ~0x03;
            if ((buf = __LDBG_malloc_str(_allocSize)) != nullptr) {
                std::fill(std::copy_n(str.c_str(), len, buf), buf + _allocSize, 0);
            }
            else {
                _allocSize = 0;
            }
        }
        else {
            buf = str.wbuffer();
            _allocSize = str.capacity();
            // register allocated block
            __LDBG_NOP_malloc(buf, _allocSize);
            std::fill(buf + len, buf + _allocSize, 0);
        }
        str.init();
        // str.setSSO(true);
        // str.setLen(0);
        // str.wbuffer()[0] = 0;
    }
    else {
        str.invalidate();
        buf = nullptr;
        _allocSize = 0;
    }
#else
    if (len) {
        // move not implemented
        _allocSize = (len + (1 << 3)) & ~0x03;
        buf = new char[_allocSize];
        std::fill(std::copy_n(_str.c_str(), len, buf), buf + _allocSize, 0);
        _str = String();
    }
    else {
        _str = String();
        buf = nullptr;
        _allocSize = 0;
    }
#endif
    if (allocSize) {
        *allocSize = _allocSize;
    }
    return buf;
}

void MoveStringHelper::move(Buffer &buf, String &&_str)
{
    int capacity;
    size_t len = _str.length();
    auto cStr = MoveStringHelper::move(std::move(_str), &capacity);
    buf.setBuffer(reinterpret_cast<uint8_t *>(cStr), capacity);
    buf.setLength(len);
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
        std::copy_n(buffer.begin(), _length, begin());
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
    MoveStringHelper::move(*this, std::move(str));
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

void Buffer::move(uint8_t **ptr)
{
    auto tmp = _buffer;
    _buffer = nullptr;
    _size = 0;
    _length = 0;
    *ptr = tmp;
    // remove from registered blocks
    __LDBG_NOP_free(tmp);
}

void Buffer::removeAndShrink(size_t index, size_t count, size_t minFree)
{
    remove(index, count);
    if (_length + minFree < _size) {
        shrink(_length);
    }
}

bool Buffer::_changeBuffer(size_t newSize)
{
    if (newSize == 0 && _size) {
        clear();
    }
    else {
        auto resize = _alignSize(newSize);
        if (resize != _size) {
            // __LDBG_printf("size=%d", _fp_size);
            if (!_buffer) {
                _buffer = __LDBG_malloc_buf(resize);
                if (!_buffer) {
                    // __LDBG_printf("alloc failed");
                    return false;
                }
            }
            else {
                _buffer = __DBG_realloc_buf(_buffer, resize);
                if (!_buffer) {
                    // __LDBG_printf("alloc failed");
                    _size = 0;
                    _length = 0;
                    return false;
                }
                if (_length > newSize) {
                    _length = newSize;
                }
            }
            _size = resize;
#if BUFFER_ZERO_FILL
            std::fill(_data_end(), _buffer_end(), 0);
#endif
        }
    }
    // __LDBG_printf("length=%d size=%d", _length, _fp_size);
    return true;
}
