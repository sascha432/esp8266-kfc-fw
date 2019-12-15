/**
* Author: sascha_lammers@gmx.de
*/

#include "Buffer.h"

Buffer::Buffer(Buffer &&buffer) {
    *this = std::move(buffer);
}

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

Buffer &Buffer::operator =(Buffer &&buffer) {
    _length = buffer._length;
    _size = buffer._size;
    _buffer = buffer._buffer;
    buffer._buffer = nullptr;
    buffer._size = 0;
    return *this;
}

Buffer &Buffer::operator =(const Buffer &buffer) {
    if (reserve(buffer._size, true)) {
        _length = buffer._length;
        memcpy(_buffer, buffer._buffer, _length);
    }
    return *this;
}

bool Buffer::equals(const Buffer &buffer) const {
    if (buffer._length != _length) {
        return false;
    }
    return memcmp(getConstChar(), buffer.getConstChar(), _length) == 0;
}

bool Buffer::operator ==(const Buffer &buffer) const {
    return equals(buffer);
}

bool Buffer::operator !=(const Buffer &buffer) const {
    return !equals(buffer);
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

void Buffer::_free() {
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
}

char *Buffer::getNulByteString() {
    _length -= write(0);
    return getChar();
}

void Buffer::setBuffer(uint8_t * buffer, size_t size) {
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
    size = (size + 0xf) & ~0xf; // 16 byte aligned
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

size_t Buffer::write_P(PGM_P data, size_t len) {
    size_t written = 0;
    while(len--) {
        auto byte = pgm_read_byte(data++);
        if (write(byte) != 1) {
            break;
        }
        written++;
    }
    return written;
}

void Buffer::remove(unsigned int index, size_t count) {
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
