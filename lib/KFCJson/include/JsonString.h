/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

// assuming malloc is allocation 16 byte blocks:

//JsonString 8 = buffer_size, malloc 16
//JsonNamedVariant<JsonString> sizeof 20, malloc 32
//JsonUnnamedVariant<JsonString> sizeof 12, malloc 16
//JsonNamedVariant<String> sizeof 40, malloc 48
//JsonUnnamedVariant<const __FlashStringHelper *> sizeof 8, malloc 16
//JsonNamedVariant<const __FlashStringHelper *> sizeof 16, malloc 16

//JsonString 16 = buffer_size, malloc 16
//JsonNamedVariant<JsonString> sizeof 36, malloc 48
//JsonUnnamedVariant<JsonString> sizeof 20, malloc 32
//JsonNamedVariant<String> sizeof 48, malloc 48
//JsonUnnamedVariant<const __FlashStringHelper *> sizeof 8, malloc 16
//JsonNamedVariant<const __FlashStringHelper *> sizeof 24, malloc 32

class JsonString {
public:
    typedef enum : uint8_t {
        STORED =                0,          // we can use the entire buffer for stored strings if we use NUL as identification and store the type in the last byte of the raw buffer
        ALLOC =                 1 << 4,
        FLASH =                 1 << 5,
        POINTER =               1 << 6,
    } StorageEnum_t;

    typedef uint16_t length_t;

    const static size_t buffer_size = 8;    // minimum size = sizeof(char *) + sizeof(length_t) + sizeof(char)

    JsonString(const JsonString &str);
    JsonString(JsonString &&str);

    JsonString();
    JsonString(const String &str);
    JsonString(const char *str);
    JsonString(const __FlashStringHelper *str);

    // do not copy to buffer even if it fits
    JsonString(const char *str, bool forceCopy);
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

protected:
    char *_allocate(length_t length);
    void _init(const char *str, length_t length);

    // inline functions to write to the _raw buffer directly to avoid alignment issues due to CPU architecture

    inline void _setType(StorageEnum_t type) {
        _raw[buffer_size - 1] = (char)type;
    }
    inline StorageEnum_t _getType() const {
        return (StorageEnum_t)_raw[buffer_size - 1];
    }

    inline const char *_getConstPtr() const {
        return *(const char **)&_raw[sizeof(length_t)];
    }
    inline char *_getPtr() {
        return *(char **)&_raw[sizeof(length_t)];
    }
    inline void _setPtr(const char *ptr) {
        *(const char **)&_raw[sizeof(length_t)] = ptr;
    }

    inline length_t _getLength() const {
        return *(length_t *)&_raw[0];
    }
    inline void _setLength(length_t length) {
        *(length_t *)&_raw[0] = length;
    }

    char _raw[buffer_size];
};
