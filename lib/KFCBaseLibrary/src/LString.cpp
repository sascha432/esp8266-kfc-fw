/**
* Author: sascha_lammers@gmx.de
*/

#include "LString.h"

LString::LString() {
    _length = 0;
}

LString::LString(const uint8_t * data, size_t length) {
    _data = data;
    _length = length;
}

bool LString::equals(const String & str) const
{
    if (str.length() != _length) {
        return false;
    }
    return strncmp(str.c_str(), getCharPtr(), _length) == 0;
}

bool LString::equalsIgnoreCase(const String & str) const
{
    if (str.length() != _length) {
        return false;
    }
    return strncasecmp(str.c_str(), getCharPtr(), _length) == 0;
}

String LString::toString() const {
    String str;
    if (_length) {
        if (str.reserve(_length)) {
            const char *ptr = getCharPtr();
            auto count = _length;
            while (count--) {
                str += *ptr++;
            }
        }
    }
    return str;
}

size_t LString::printTo(Print & p) const {
    if (!_length) {
        return 0;
    }
    return p.write(_data, _length);
}

char * LString::strcpy(char * dst)
{
    if (dst) {
        strncpy(dst, getCharPtr(), _length)[_length] = 0;
    }
    return dst;
}

char * LString::strcat(char * dst)
{
    if (dst) {
        strncpy(&dst[strlen(dst)], getCharPtr(), _length)[_length] = 0;
    }
    return dst;
}

LString::errno_t LString::strcpy_s(char * dst, size_t destSize)
{
    if (dst && destSize > 1) {
        auto space = destSize - 1;
        if (_length <= space) {
            strncpy(dst, getCharPtr(), _length)[_length] = 0;
            return 0;
        }
        strncpy(dst, getCharPtr(), space)[space] = 0;
    }
    return 1;
}

LString::errno_t LString::strcat_s(char * dst, size_t destSize)
{
    if (dst) {
        auto len = strlen(dst);
        auto space = len + destSize - 1;
        if (_length <= space) {
            strncpy(dst + len, getCharPtr(), _length)[_length] = 0;
            return 0;
        }
        strncpy(dst + len, getCharPtr(), space)[space] = 0;
    }
    return 1;
}
