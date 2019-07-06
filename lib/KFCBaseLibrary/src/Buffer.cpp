/**
* Author: sascha_lammers@gmx.de
*/

#include "Buffer.h"

Buffer::Buffer(size_t size) {
    _buffer = nullptr;
    _size = size;
    _length = 0;
    if (_size) {
        _buffer = (uint8_t *)malloc(_size);
        if (!_buffer) {
            size = 0;
        }
    }
}

Buffer::~Buffer() {
    _free();
}

void Buffer::clear() {
    _free();
    _size = 0;
    _length = 0;
}

uint8_t *Buffer::dupClear() { // return unfreed buffer
    uint8_t *tmp = _buffer;
    _buffer = nullptr;
    _size = 0;
    _length = 0;
    return tmp;
}

uint8_t * Buffer::get() const {
    return _buffer;
}

const uint8_t * Buffer::getConst() const {
    return _buffer;
}

char * Buffer::getChar() const {
    return (char *)_buffer;
}

const char * Buffer::getBuffer() const {
    return getConstChar();
}

const char * Buffer::getConstChar() const {
    return (const char *)_buffer;
}

void Buffer::_free() {
    free(_buffer);
    _buffer = nullptr;
}

char *Buffer::getNulByteString() {
    _length -= write(0);
    return (char *)_buffer;
}

void Buffer::setBuffer(uint8_t * buffer, size_t size)
{
    _free();
    _buffer = buffer;
    _size = size;
}

size_t Buffer::size() const {
    return _size;
}

size_t Buffer::length() const {
    return _length;
}

bool Buffer::reserve(size_t size, bool shrink) {
    if (shrink || size < _size) {
        return true;
    }
    if (shrink && _length == 0) {
        _free();
        return true;
    }
    size += size % Buffer::MALLOC_PADDING;
    if (!(_buffer = (uint8_t *)realloc(_buffer, size))) {
        _size = 0;
        _length = 0;
        return false;
    }
    _size = size;
    return true;
}

bool Buffer::available(size_t len) {
    return (len + _length <= _size);
}

String Buffer::toString() const {
#if 1 || _WIN32 || _WIN64
    String tmp;
    tmp.reserve(_length);
    for (size_t i = 0; i < _length; i++) {
        tmp += (char)_buffer[i];
    }
#else
    String tmp;
    if (tmp.reserve(_length)) {
        strncpy(tmp.begin(), (char *)_buffer, _length);
        tmp.setLen(_length);
}
#endif
    return tmp;
    }

Buffer::operator String() const {
    return toString();
}

size_t Buffer::write(uint8_t *data, size_t len) {
    if (_length + len > _size) {
        if (!reserve(_length + len)) {
            return 0;
        }
    }
    memmove(_buffer + _length, data, len);
    _length += len;
    return len;
}

void Buffer::remove(unsigned int index, size_t count) {
#if 0
    size_t _index = index;
    size_t _count = count;
    size_t __length = _length;
    char buf[_length + 1];
    if (_length) {
        memcpy(buf, _buffer, _length);
        buf[_length - 1] = 0;
    } else {
        *buf = 0;
    }
#endif
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
    memmove(_buffer + index, _buffer + index + count, _length - index);
#if 0
    char buf2[_length + 1];
    if (_length) {
        memcpy(buf2, _buffer, _length);
        buf2[_length - 1] = 0;
    } else {
        *buf2 = 0;
    }
    debug_printf_P(PSTR("remove(%u, %u)=(%u, %u) len %d,%d string '%s','%s'\n"), _index, _count, index, count, __length, _length, buf, buf2);
#endif


}

void Buffer::removeAndShrink(unsigned int index, size_t count) {
    remove(index, count);
    if (_length < _size + 256) {
        reserve(_length, true);
    }
}


Buffer &Buffer::operator+=(char data) {
    write(data);
    return  *this;
}

Buffer &Buffer::operator+=(uint8_t data) {
    write(data);
    return *this;
}

Buffer& Buffer::operator+=(const char *data) {
    write((uint8_t *)data, strlen(data));
    return *this;
}

Buffer& Buffer::operator+=(const String &data) {
    write((uint8_t *)data.c_str(), data.length());
    return *this;
}
