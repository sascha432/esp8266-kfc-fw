/**
* Author: sascha_lammers@gmx.de
*/

#include "PrintStaticString.h"

PrintStaticString::PrintStaticString(char * buffer, size_t size, const char * str) : PrintStaticString(buffer, size) {
    print(str);
}

PrintStaticString::PrintStaticString(char * buffer, size_t size, const __FlashStringHelper * str) : PrintStaticString(buffer, size) {
    print(str);
}

PrintStaticString::PrintStaticString(char * buffer, size_t size) : Print(), _buffer(buffer), _size(size), _position(0) {
    *_buffer = 0;
}

String PrintStaticString::toString() const {
    // __String tmp(_position);
    // tmp.__copy((const uint8_t *)_buffer);
    return String(_buffer);
}

PrintStaticString::operator String() const {
    return toString();
}

const char * PrintStaticString::getBuffer() {
    return _buffer;
}

size_t PrintStaticString::size() const {
    return _size;
}

size_t PrintStaticString::length() const {
    return _position;
}

size_t PrintStaticString::write(uint8_t data) {
    if (_position < _size - 1) {
        _buffer[_position] = data;
        _position++;
        _buffer[_position] = 0;
        return sizeof(data);
    }
    return 0;
}

size_t PrintStaticString::write(const uint8_t * buffer, size_t size) {
    int _space = _size - _position - 1;
    if (_space <= 0) {
        return 0;
    }
    if ((int)size > _space) {
        size = _space;
    }
    char *dst = _buffer + _position;
    char *src = (char *)buffer;
    _position += size;
    while (size--) {
        *dst++ = *src++;
    }
    *dst = 0;
    return size;
}
