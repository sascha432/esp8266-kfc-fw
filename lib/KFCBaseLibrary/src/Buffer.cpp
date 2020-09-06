/**
* Author: sascha_lammers@gmx.de
*/

#include <PrintString.h>
#include "debug_helper.h"
#include "Buffer.h"

#if 0
#include "debug_helper_enable.h"
#include "debug_helper_enable_mem.h"
#else
#include "debug_helper_disable.h"
#include "debug_helper_disable_mem.h"
#endif

MoveStringHelper::MoveStringHelper()
{
}

void MoveStringHelper::move(Buffer &buf, String &&_str)
{
    auto &&str = static_cast<MoveStringHelper &&>(_str);
    __LDBG_println();
#if ESP8266
    if (str.length()) {
#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
        if (str.isSSO()) {
#else
        if (str.sso()) {
#endif
            buf.write(str);
        }
        else {
            buf.setBuffer((uint8_t *)str.wbuffer(), str.capacity());
            buf.setLength(str.length());
            // register allocated block
            __LDBG_NOP_malloc(str.wbuffer(), str.capacity());
            IF_DEBUG_BUFFER_ALLOC(buf._alloc++);
        }
        str.init();
        // str.setSSO(true);
        // str.setLen(0);
        // str.wbuffer()[0] = 0;
    }
#else
    // move not implemented
    buf.write(str);
    str = MoveStringHelper();
#endif
}

Buffer::Buffer() :
    _buffer(nullptr),
    _length(0),
    _size(0)
#if DEBUG_BUFFER_ALLOC
   ,_realloc(0),
    _alloc(0),
    _free(0)
#endif
{
}

Buffer::Buffer(Buffer &&buffer) noexcept :
    _buffer(std::exchange(buffer._buffer, nullptr)),
    _length(std::exchange(buffer._length, 0)),
    _size(std::exchange(buffer._size, 0))
#if DEBUG_BUFFER_ALLOC
    ,_realloc(std::exchange(buffer._realloc, 0)),
    _alloc(std::exchange(buffer._alloc, 0)),
    _free(std::exchange(buffer._free, 0))
#endif
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
        IF_DEBUG_BUFFER_ALLOC(_free++);
    }
    CHECK_MEMORY();
}

Buffer::Buffer(const __FlashStringHelper *str) : Buffer()
{
    write(str, strlen_P(reinterpret_cast<PGM_P>(str)));
}

Buffer::Buffer(String &&str) : Buffer()
{
    MoveStringHelper::move(*this, std::move(str));
}

Buffer::Buffer(const String &str) : Buffer()
{
    write(str);
}

Buffer &Buffer::operator =(Buffer &&buffer) noexcept
{
    if (_buffer) {
        __LDBG_free(_buffer);
        IF_DEBUG_BUFFER_ALLOC(_free++);
    }
    _buffer = buffer._buffer;
    _length = buffer._length;
    _size = buffer._size;
    buffer._buffer = nullptr;
    buffer._length = 0;
    buffer._size = 0;
#if DEBUG_BUFFER_ALLOC
    _realloc = std::exchange(buffer._realloc, 0);
    _alloc = std::exchange(buffer._alloc, 0);
    _free = std::exchange(buffer._free, 0);
#endif
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

void Buffer::clear()
{
    // __LDBG_printf("len=%u size=%u ptr=%p", _length, _fp_size, _buffer);
    if (_buffer) {
        __LDBG_free(_buffer);
        IF_DEBUG_BUFFER_ALLOC(_free++);
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
    // remove from registered blocks
    __LDBG_NOP_free(tmp);
    IF_DEBUG_BUFFER_ALLOC(_free++);
}

void Buffer::setBuffer(uint8_t *buffer, size_t size)
{
    clear();
    _buffer = buffer;
    _size = size;
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
    if (reserve(_length + len)) {
        memmove(_buffer + _length, data, len);
        _length += len;
        return len;
    }
    return 0;
}

size_t Buffer::write_P(PGM_P data, size_t len)
{
    // __LDBG_printf("len=%d", len);
    if (reserve(_length + len)) {
        memcpy_P(_buffer + _length, data, len);
        _length += len;
        return len;
    }
    return 0;
}

int Buffer::read()
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

int Buffer::peek()
{
    if (_length) {
        return *_buffer;
    }
    return -1;
}

void Buffer::remove(size_t index, size_t count)
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

void Buffer::_remove(size_t index, size_t count)
{
    auto dst_begin = begin() + index;
    std::copy(dst_begin + count, end(), dst_begin);
    _length -= count;
#if BUFFER_ZERO_FILL
    std::fill(begin() + _length, _buffer_end(), 0);
#endif
}

#if DEBUG_BUFFER_ALLOC

void Buffer::dumpAlloc(Print &output)
{
    output.printf_P(PSTR("(re-)/alloc/free=%u %u %u len/size=%u"), _realloc, _alloc, _free, length(), size());
}

#endif

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
                IF_DEBUG_BUFFER_ALLOC(_alloc++);
            }
            else {
                _buffer = __DBG_realloc_buf(_buffer, resize);
                if (!_buffer) {
                    // __LDBG_printf("alloc failed");
                    _size = 0;
                    _length = 0;
                    return false;
                }
                IF_DEBUG_BUFFER_ALLOC(_realloc++);
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
