/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class JsonString {
public:
    typedef enum : uint8_t {
        STORED =                0,          // we can use the entire buffer for stored strings if we use NUL as identification
        ALLOC =                 1 << 4,
        FLASH =                 1 << 5,
        POINTER =               1 << 6,
    } StorageEnum_t;

    typedef struct {
        uint32_t length;
        char *ptr;
    } _str_t;

    const static size_t buffer_size = 16;

    JsonString(const JsonString &str);
    JsonString(JsonString &&str);

    JsonString();
    JsonString(const String &str);
    JsonString(const char *str, bool forceCopy);
    JsonString(const char *str);
    JsonString(const __FlashStringHelper *str);
    JsonString(const __FlashStringHelper *str, bool forceFlash);
    ~JsonString();

    JsonString &operator =(const JsonString &str);
    JsonString &operator =(JsonString &&str);

    bool operator ==(const char *str) const;
    bool operator ==(const String &str) const;
    bool operator ==(const JsonString &str) const;
    bool operator ==(const __FlashStringHelper *str) const;

    bool equals(const char *str) const;
    bool equals(const String &str) const;
    bool equals(const JsonString &str) const;
    bool equals(const __FlashStringHelper *str) const;

    void clear();
    size_t length() const;
    bool isProgMem() const;
    size_t printTo(Print &p) const;

    const char *getPtr() const;
    const __FlashStringHelper *getFPStr() const {
        return reinterpret_cast<const __FlashStringHelper *>(getPtr());
    }
    String toString() const;

    int getStructSize() const {
        return sizeof(_str);
    }

protected:
    char *_allocate(size_t length);
    void _init(const char *str, size_t length);

    inline void _setType(StorageEnum_t type) {
        _raw[buffer_size - 1] = (char)type;
    }
    inline StorageEnum_t _getType() const {
        return (StorageEnum_t)_raw[buffer_size - 1];
    }

    union {
        _str_t _str;
        char _raw[buffer_size];
    };
};
