/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonString.h"

JsonString::JsonString() {
    _setType(STORED);
    *_raw = 0;
}

JsonString::JsonString(const String & str) {
    _init(str.c_str(), str.length());
}

JsonString::JsonString(const char * str, bool forceCopy) {
    _init(str, strlen(str));
}

JsonString::JsonString(const char * str) {
    if (*str) {
        _setType(POINTER);
        _str.ptr = (char *)str;
        _str.length = strlen(str);
    }
    else {
        _setType(STORED);
        *_raw = 0;
    }
}

JsonString::JsonString(const __FlashStringHelper * str) {
    // while checking string length we can copy already from slow flash
    PGM_P lptr = reinterpret_cast<PGM_P>(str);
    auto dptr = _raw;
    uint32_t length = 0;
    while ((*dptr++ = (char)pgm_read_byte(lptr++)) != 0) {
        length++;
        if (length >= buffer_size - 1) { // too much, add rest of length and continue
            length += strlen_P(lptr);
            break;
        }
    }
    if (length < buffer_size) {
        _setType(STORED);
    }
    else {
        _setType(FLASH);
        _str.ptr = (char *)str;
        _str.length = length;
    }
}

JsonString::JsonString(const __FlashStringHelper * str, bool forceFlash) {
    _setType(FLASH);
    _str.ptr = (char *)str;
    _str.length = strlen_P(reinterpret_cast<PGM_P>(str));
}

JsonString::~JsonString() {
    if (_getType() == ALLOC) {
        free(_str.ptr);
    }
}

bool JsonString::operator==(const char * str) const {
    return equals(str);
}

bool JsonString::operator==(const String & str) const {
    return equals(str);
}

bool JsonString::operator==(const JsonString & str) const {
    return equals(str);
}

bool JsonString::operator==(const __FlashStringHelper * str) const {
    return equals(str);
}

bool JsonString::equals(const char * str) const {
    if (isProgMem()) {
        return strcmp_P(str, getPtr()) == 0;
    }
    return strcmp(str, getPtr()) == 0;
}

bool JsonString::equals(const String & str) const {
    if (isProgMem()) {
        return strcmp_P(str.c_str(), getPtr()) == 0;
    }
    return strcmp(str.c_str(), getPtr()) == 0;
}

bool JsonString::equals(const JsonString & str) const {
    if (isProgMem()) {
        if (str.isProgMem()) {
            return strcmp_P_P(str.getPtr(), getPtr()) == 0;
        }
        else {
            return strcmp_P(str.getPtr(), getPtr()) == 0;
        }
    }
    return strcmp(str.getPtr(), getPtr()) == 0;
}

bool JsonString::equals(const __FlashStringHelper *str) const {
    if (isProgMem()) {
        return strcmp_P_P(getPtr(), reinterpret_cast<PGM_P>(str)) == 0;
    }
    return strcmp_P(getPtr(), reinterpret_cast<PGM_P>(str)) == 0;
}

void JsonString::clear() {
    if (_getType() == ALLOC) {
        free(_str.ptr);
    }
    _setType(STORED);
    *_raw = 0;
}

size_t JsonString::length() const {
    if (_getType() == STORED) {
        return strlen(_raw);
    }
    return _str.length;
}

String JsonString::toString() const {
    if (_getType() == FLASH) {
        return String(FPSTR(_str.ptr));
    }
    else if (_getType() == STORED) {
        return String(_raw);
    }
    return String(_str.ptr);
}

bool JsonString::isProgMem() const {
    return _getType() == FLASH;
}

const char * JsonString::getPtr() const {
    if (_getType() == STORED) {
        return _raw;
    }
    return _str.ptr;
}

size_t JsonString::printTo(Print & p) const {
    if (_getType() == FLASH) {
        return p.print(FPSTR(_str.ptr));
    }
    else if (_getType() == STORED) {
        return p.write(_raw, strlen(_raw));
    }
    return p.write(_str.ptr, length());
}

char * JsonString::_allocate(size_t length) {
    _setType(ALLOC);
    return _str.ptr = (char *)malloc(length + 1);
}

void JsonString::_init(const char * str, size_t length) {
    if (length < buffer_size) {
        _setType(STORED);
        strncpy(_raw, str, length)[length] = 0;
    }
    else {
        strncpy(_allocate(length), str, length)[length] = 0;
        _str.length = length;
    }
}
