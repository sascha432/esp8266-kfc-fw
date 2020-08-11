/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonString.h"

JsonString::JsonString(const JsonString &str) : JsonString()
{
   *this = str;
}

JsonString::JsonString(JsonString &&str) : JsonString()
{
   *this = std::move(str);
}

JsonString::JsonString()
{
    _setType(STORED);
    *_raw = 0;
}

JsonString::JsonString(const String &str)
{
    _init(str.c_str(), (length_t)str.length());
}

JsonString::JsonString(const char *str, bool forceCopy)
{
    _init(str, (length_t)strlen(str));
}

JsonString::JsonString(const char *str)
{
    if (*str) {
        _setType(POINTER);
        _setPtr(str);
        _setLength((length_t)strlen(str));
    }
    else {
        _setType(STORED);
        *_raw = 0;
    }
}

JsonString::JsonString(char ch)
{
    _init(&ch, 1);
}

JsonString::JsonString(const __FlashStringHelper *str)
{
    // while checking string length we can copy already from slow flash
    PGM_P lptr = RFPSTR(str);
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
        _setPtr(reinterpret_cast<const char *>(str));
        _setLength(length);
    }
}

JsonString::JsonString(const __FlashStringHelper *str, bool forceFlash)
{
    _setType(FLASH);
    _setPtr(reinterpret_cast<const char *>(str));
    _setLength((length_t)strlen_P(RFPSTR(str)));
}

JsonString::~JsonString()
{
    if (_getType() == ALLOC) {
        free(_getPtr());
    }
}

JsonString &JsonString::operator=(const JsonString &str)
{
    if (_getType() == ALLOC) {
        free(_getPtr());
    }
    if (str._getType() == ALLOC) {
        _init(str._getConstPtr(), str._getLength());
    }
    else {
        memmove(_raw, str._raw, sizeof(_raw));
    }
    return *this;
}

JsonString & JsonString::operator=(JsonString &&str)
{
    if (_getType() == ALLOC) {
        free(_getPtr());
    }
    if (str._getType() == ALLOC) {
        _setType(ALLOC);
        _setPtr(str._getPtr());
        _setLength(str._getLength());
    }
    else {
        memmove(_raw, str._raw, sizeof(_raw));
    }
    str._setType(STORED);
    *str._raw = 0;
    return *this;
}

bool JsonString::operator==(const char *str) const
{
    return equals(str);
}

bool JsonString::operator==(const String &str) const
{
    return equals(str);
}

bool JsonString::operator==(const JsonString &str) const
{
    return equals(str);
}

bool JsonString::operator==(const __FlashStringHelper *str) const
{
    return equals(str);
}

bool JsonString::equals(const char *str) const
{
    if (isProgMem()) {
        return strcmp_P(str, _getConstPtr()) == 0;
    }
    return strcmp(str, getPtr()) == 0;
}

bool JsonString::equals(const String &str) const
{
    if (isProgMem()) {
        return strcmp_P(str.c_str(), getPtr()) == 0;
    }
    return strcmp(str.c_str(), getPtr()) == 0;
}

bool JsonString::equals(const JsonString &str) const
{
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

bool JsonString::equals(const __FlashStringHelper *str) const
{
    if (isProgMem()) {
        return strcmp_P_P(getPtr(), RFPSTR(str)) == 0;
    }
    return strcmp_P(getPtr(), RFPSTR(str)) == 0;
}

void JsonString::clear()
{
    if (_getType() == ALLOC) {
        free(_getPtr());
    }
    _setType(STORED);
    *_raw = 0;
}

size_t JsonString::length() const
{
    if (_getType() == STORED) {
        return strlen(_raw);
    }
    return _getLength();
}

String JsonString::toString() const
{
    if (_getType() == FLASH) {
        return String(FPSTR(_getConstPtr()));
    }
    else if (_getType() == STORED) {
        return String(_raw);
    }
    return String(_getConstPtr());
}

bool JsonString::isProgMem() const
{
    return _getType() == FLASH;
}

const char *JsonString::getPtr() const
{
    if (_getType() == STORED) {
        return _raw;
    }
    return _getConstPtr();
}

size_t JsonString::printTo(Print &p) const
{
    if (_getType() == FLASH) {
        return p.print(FPSTR(_getConstPtr()));
    }
    else if (_getType() == STORED) {
        return p.write(_raw, strlen(_raw));
    }
    return p.write(_getConstPtr(), _getLength());
}

char *JsonString::_allocate(length_t length)
{
    _setType(ALLOC);
    auto ptr = (char *)malloc(length + 1);
    _setPtr(ptr);
    return ptr;
}

void JsonString::_init(const char *str, length_t length)
{
    if (length < buffer_size) {
        _setType(STORED);
        strncpy(_raw, str, length)[length] = 0;
    }
    else {
         strncpy(_allocate(length), str, length)[length] = 0;
         _setLength(length);
    }
}
