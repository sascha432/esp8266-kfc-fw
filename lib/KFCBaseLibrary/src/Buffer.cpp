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
