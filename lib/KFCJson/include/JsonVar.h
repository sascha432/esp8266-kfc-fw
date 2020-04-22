/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonBaseReader.h"

// JSON value storage using by JsonBaseReader
#if _MSC_VER
#undef TRUE
#undef FALSE
#endif

class JsonVar {
public:
    typedef enum : uint8_t {
        // 4 bit enumeration
        INVALID =           0,
        INT =               1,
        FLOAT =             2,
        DOUBLE =            3,          // unused
        TYPE_MASK =         0xf,

        // flags
        EXPONENT =          1 << 4,     // E notation
        NEGATIVE =          1 << 5,     // number is negative
        SHORT =             1 << 6,     // unused
        LONG =              1 << 7,     // unused
    } NumberType_t;

    typedef JsonBaseReader::JsonType_t JsonType_t;

    enum class BooleanValueType {
        INVALID = -1,
        FALSE = 0,
        TRUE = 1,
    };

    JsonVar();
    JsonVar(JsonType_t type);
    JsonVar(JsonType_t type, const String &value);

    void setValue(const String &value);
    String getValue() const;
    BooleanValueType getBooleanValue() const;
    JsonBaseReader::JsonType_t getType() const;

    // format JSON Variant
    static String formatValue(const String &value, JsonType_t type);

    // validate JSON number and return type information
    static uint8_t getNumberType(const char * value);

    // this can be used to convert the string after it has been validated with getNumberType()
    static double getDouble(const char *value);
    static long getInt(const char *value, bool _unsigned = false);
    inline static unsigned long getUnsigned(const char *value) {
        return (unsigned long)getInt(value, true);
    }

protected:
    JsonBaseReader::JsonType_t _type;
    String _value;
};
