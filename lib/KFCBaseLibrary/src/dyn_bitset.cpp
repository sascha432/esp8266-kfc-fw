/**
 * Author: sascha_lammers@gmx.de
 */

#include "dyn_bitset.h"

dynamic_bitset::dynamic_bitset() : dynamic_bitset(0, 0, 0) {
}

dynamic_bitset::dynamic_bitset(uint8_t maxSize) : dynamic_bitset(0, maxSize, maxSize) {
}

dynamic_bitset::dynamic_bitset(uint8_t size, uint8_t maxSize) : dynamic_bitset(0, size, maxSize) {
}

dynamic_bitset::dynamic_bitset(uint32_t value, uint8_t size, uint8_t maxSize) {
    _buffer = nullptr;
    _maxSize = maxSize;
    _size = size;
    if (_maxSize) {
        _init();
        setValue(value);
    }
}

dynamic_bitset::~dynamic_bitset() {
    if (_buffer) {
        free(_buffer);
    }
}

void dynamic_bitset::setBytes(uint8_t * bytes, uint8_t len) {
    memcpy(_buffer, bytes, std::min(len, (uint8_t)((_maxSize + 7) >> 3)));
}

const uint8_t * dynamic_bitset::getBytes() const {
    return _buffer;
}

String dynamic_bitset::getBytesAsString() const {
    String output;
    char buf[16];
    for (int8_t i = ((_size + 7) >> 3) - 1; i >= 0; i--) {
        snprintf(buf, sizeof(buf), "\\x%02x", ((unsigned)_buffer[i]) & 0xff);
        output += buf;
    }
    return output;
}

void dynamic_bitset::setValue(uint32_t value) {
    memcpy(_buffer, &value, std::min((int)sizeof(value), ((_maxSize + 7) >> 3)));
}

const uint32_t dynamic_bitset::getValue() const {
    uint32_t value;
    memcpy(&value, _buffer, std::min((int)sizeof(value), ((_maxSize + 7) >> 3)));
    return value;
}

void dynamic_bitset::setString(const char * str, size_t len) {
    int offset = 0;
    if (len > _maxSize) {
        offset = len - _maxSize;
        len -= offset;
    }
    else if (len < _maxSize) {
        // reusing offset variable. clear bits in area that isn't copied
        //offset = ((_maxSize - len) + 7) >> 3;
        //         memset(&_buffer[((_maxSize + 7) >> 3) - offset], 0, offset);
        memset(_buffer, 0, ((_maxSize + 7) >> 3));
        offset = 0;
    }
    str += offset;
    while (len--) {
        set((uint8_t)len, *str++ == '1');
    }
}

void dynamic_bitset::setSize(uint8_t size) {
    _size = size;
    if (_size > _maxSize) {
        _size = _maxSize;
    }
}

const uint8_t dynamic_bitset::size() const {
    return _size;
}

void dynamic_bitset::setMaxSize(uint8_t maxSize) {
    _maxSize = maxSize;
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
    if (_maxSize) {
        _size = _maxSize;
        _init();
    }
    else {
        _size = 0;
    }
}

const uint8_t dynamic_bitset::getMaxSize() const {
    return _maxSize;
}

String dynamic_bitset::toString() const {
    String output;
    output.reserve(_size);
    for (int i = _size - 1; i >= 0; i--) {
        output += (char)(test(i) ? 49 : 48);
    }
    return output;
}

void dynamic_bitset::set(uint8_t pos, bool value) {  // zero index, first bit is 0
    if (pos < _maxSize) {
        uint8_t mask = (1 << pos % 8);
        if (value) {
            _buffer[pos >> 3] |= mask;
        }
        else {
            _buffer[pos >> 3] &= ~mask;
        }
    }
}

bool dynamic_bitset::test(uint8_t pos) const {  // zero index, first bit is 0
    if (pos < _maxSize) {
        return _buffer[pos >> 3] & (1 << pos % 8);
    }
    return false;
}

void dynamic_bitset::_init() {
    _size = std::min(_size, _maxSize);
    _buffer = (uint8_t *)calloc(((_maxSize + 7) >> 3), 1);
}

 dynamic_bitset::bit_helper::bit_helper(dynamic_bitset * bitset, uint8_t index) {
     _index = index;
     _bitset = bitset;
 }
