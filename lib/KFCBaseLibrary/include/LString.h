/**
* Author: sascha_lammers@gmx.de
*/

// LString - points to memory that contains a string with a fixed length and without a NUL byte
// for printf formating, use printf("%*.*s", str.length(), str.length(), str.getCharPtr()) (OR [preferably not] printf("%s", str.toString().c_str()))

#pragma once

#include <Arduino_compat.h>

class LString : public Printable {
public:
    typedef int errno_t;

    LString();
    LString(const uint8_t *data, size_t length);
    inline LString(const char *str, size_t length) : LString(reinterpret_cast<const uint8_t *>(str), length) {
    }

    bool equals(const String &str) const;
    bool equalsIgnoreCase(const String &str) const;

    String toString() const;
    virtual size_t printTo(Print &p) const;

    char *strcpy(char *dst);
    char *strcat(char *dst);
    errno_t strcpy_s(char *dst, size_t destSize);
    errno_t strcat_s(char *dst, size_t destSize); 

    inline size_t length() const {
        return _length;
    }

    inline const uint8_t *getData() const {
        return _data;
    }

    inline const char *getCharPtr() const {
        return reinterpret_cast<const char *>(_data);
    }

private:
    const uint8_t *_data;
    size_t _length;
};
