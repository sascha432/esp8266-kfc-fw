/**
 * Author: sascha_lammers@gmx.de
 */

#include "dyn_bitset.h"
#include <PrintString.h>
#include <algorithm>


dynamic_bitset::dynamic_bitset() : _size(0), _maxSize(0), _buffer(nullptr) {
}

dynamic_bitset::dynamic_bitset(uint8_t maxSize) : dynamic_bitset((uint32_t)0, maxSize, maxSize) {
}

dynamic_bitset::dynamic_bitset(uint8_t size, uint8_t maxSize) : dynamic_bitset((uint32_t)0, size, maxSize) {
}

dynamic_bitset::dynamic_bitset(uint32_t value, uint8_t size, uint8_t maxSize) : _size(size), _maxSize(maxSize), _buffer(nullptr) {
    if (_maxSize) {
        _init();
        setValue(value);
    }
}

dynamic_bitset::dynamic_bitset(const char *fromBytes, uint8_t length, uint8_t size) {
    setMaxSize(size);
    setBytes((uint8_t *)fromBytes, length);
}

dynamic_bitset::~dynamic_bitset() {
    if (_buffer) {
        delete[] _buffer;
    }
}

void dynamic_bitset::setBytes(uint8_t *bytes, uint8_t len) {
    memcpy(_buffer, bytes, std::min(len, (uint8_t)((_maxSize + 7) >> 3)));
}

const uint8_t * dynamic_bitset::getBytes() const {
    return _buffer;
}

String dynamic_bitset::getBytesAsString() const {
    PrintString output;
    for (int8_t i = ((_size + 7) >> 3) - 1; i >= 0; i--) {
        output.printf("\\x%02x", ((unsigned)_buffer[i]) & 0xff);
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
        // reusing offset variable. clear bits in area that won't be copied
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
        delete[] _buffer;
        _buffer = nullptr;
    }
    if (_maxSize) {
        _size = _maxSize;
        _init();
    } else {
        _size = 0;
    }
}

const uint8_t dynamic_bitset::getMaxSize() const {
    return _maxSize;
}

void dynamic_bitset::fromString(const char *pattern, bool reverse) {
    uint8_t len = (uint8_t)strlen(pattern);
    setMaxSize(len);
    setSize(_maxSize);
    uint8_t start, end, pos = 0;

    if (reverse) {
        start = len - 1;
        end = 0;
        while (start >= end) {
            set(pos++, pattern[start] == '1');
            start--;
        }
    } else {
        start = 0;
        end = len;
        while (start < end) {
            set(pos++, pattern[start] == '1');
            start++;
        }
    }
}

String dynamic_bitset::toString() const {
    String output;
    output.reserve(_size);
    for (int i = _size - 1; i >= 0; i--) {
        output += (test(i) ? '1' : '0');
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

String dynamic_bitset::generateCode(const char *variable, bool setBytes) const {

    PrintString code;
    uint8_t numBytes = ((_size + 7) >> 3);
    if (setBytes) {
        code.printf("%s.setMaxSize(%d);\n", variable, _size);
        if (numBytes > 4) {
            code.printf("%s.setBytes((uint8_t *)\"%s\", %d);\n", variable, getBytesAsString().c_str(), numBytes);
        } else {
            code.printf("%s.setValue(0x%x);\n", variable, getValue());
        }
    } else {
        if (numBytes > 4) {
            code.printf("dynamic_bitset %s(\"%s\", %d, %d);\n", variable, getBytesAsString().c_str(), numBytes, _size);
        } else {
            code.printf("dynamic_bitset %s(%s0x%x, %d, %d);\n", variable, getValue() ? "" : "(uint32_t)", getValue(), _size, _size);
        }
    }

    return code;
}

void dynamic_bitset::_init()
{
    _size = std::min(_size, _maxSize);
    auto sz = ((_maxSize + 7) >> 3);
    _buffer = new uint8_t[sz];
    std::fill_n(_buffer, sz, 0);
}

 dynamic_bitset::bit_helper::bit_helper(dynamic_bitset *bitset, uint8_t index) {
     _index = index;
     _bitset = bitset;
 }
