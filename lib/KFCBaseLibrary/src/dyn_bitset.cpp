/**
 * Author: sascha_lammers@gmx.de
 */

#include "dyn_bitset.h"
#include <PrintString.h>
#include <algorithm>

String dynamic_bitset::getBytesAsString() const
{
    PrintString output;
    for (int8_t i = ((_size + 7) >> 3) - 1; i >= 0; i--) {
        output.printf_P(PSTR("\\x%02x"), static_cast<uint8_t>(_buffer[i]));
    }
    return output;
}

void dynamic_bitset::setValue(uint32_t value)
{
    memcpy(_buffer, &value, std::min<size_t>(sizeof(value), ((_maxSize + 7) >> 3)));
}

const uint32_t dynamic_bitset::getValue() const
{
    uint32_t value = 0;
    memcpy(&value, _buffer, std::min<size_t>(sizeof(value), ((_maxSize + 7) >> 3)));
    return value;
}

void dynamic_bitset::setString(const char * str, size_t len)
{
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

void dynamic_bitset::setMaxSize(uint8_t maxSize)
{
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

void dynamic_bitset::fromString(const char *pattern, bool reverse)
{
    uint8_t len = (uint8_t)strlen(pattern);
    setMaxSize(len);
    setSize(_maxSize);
    auto endPtr = pattern + len;
    uint8_t pos = reverse ? len - 1 : 0;
    while (pattern < endPtr) {
        set(pos, *pattern++ == '1');
        reverse ? --pos : ++pos;
    }
}

String dynamic_bitset::toString() const
{
    String output;
    output.reserve(_size);
    for (int i = _size - 1; i >= 0; i--) {
        output += (test(i) ? '1' : '0');
    }
    return output;
}

void dynamic_bitset::set(uint8_t pos, bool value)  // zero index, first bit is 0
{
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

bool dynamic_bitset::test(uint8_t pos) const // zero index, first bit is 0
{
    if (pos < _maxSize) {
        return _buffer[pos >> 3] & (1 << pos % 8);
    }
    return false;
}

String dynamic_bitset::generateCode(const char *variable, bool setBytes) const
{
    PrintString code;
    uint8_t numBytes = ((_size + 7) >> 3);
    if (setBytes) {
        code.printf_P(PSTR("%s.setMaxSize(%d);\n"), variable, _size);
        if (numBytes > 4) {
            code.printf_P(PSTR("%s.setBytes((uint8_t *)\"%s\", %d);\n"), variable, getBytesAsString().c_str(), numBytes);
        } else {
            code.printf_P(PSTR("%s.setValue(0x%x);\n"), variable, getValue());
        }
    } else {
        if (numBytes > 4) {
            code.printf_P(PSTR("dynamic_bitset %s(\"%s\", %d, %d);\n"), variable, getBytesAsString().c_str(), numBytes, _size);
        } else {
            code.printf_P(PSTR("dynamic_bitset %s(%s0x%x, %d, %d);\n"), variable, getValue() ? PSTR("") : PSTR("(uint32_t)"), getValue(), _size, _size);
        }
    }

    return code;
}

void dynamic_bitset::_init()
{
    _size = std::min(_size, _maxSize);
    auto sz = ((_maxSize + 7) >> 3);
    _buffer = new uint8_t[sz]();
}

