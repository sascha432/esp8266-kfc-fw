/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonVar.h"
#include "JsonTools.h"
#include <PrintString.h>

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

JsonVar::JsonVar() {
    _type = JsonBaseReader::JSON_TYPE_INVALID;
}

JsonVar::JsonVar(JsonType_t type) {
    _type = type;
}

JsonVar::JsonVar(JsonType_t type, const String &value) {
    _type = type;
    _value = value;
}

void JsonVar::setValue(const String &value) {
    _value = value;
}

String JsonVar::getValue() const {
    return _value;
}

JsonVar::JsonType_t JsonVar::getType() const {
    return _type;
}

String JsonVar::formatValue(const String &value, JsonType_t type) {
    switch (type) {
    case JsonBaseReader::JSON_TYPE_NULL:
        return FSPGM(null);
    case JsonBaseReader::JSON_TYPE_BOOLEAN:
#if DEBUG
        if (strcasecmp_P(value.c_str(), SPGM(true)) != 0 && strcasecmp_P(value.c_str(), SPGM(false)) != 0) {
            debug_printf_P(PSTR("Invalid JSON value '%s'\n"), value.c_str());
            panic();
        }
#endif
        if (value.length() == 4) { // its true or false
        //if (strcasecmp_P(value.c_str(), SPGM(true)) == 0) {
            return FSPGM(true);
        }
        return FSPGM(false);
    case JsonBaseReader::JSON_TYPE_INT:
        return String(value.toInt());
    case JsonBaseReader::JSON_TYPE_FLOAT:
        return String(value.toFloat());
    case JsonBaseReader::JSON_TYPE_NUMBER: {
        PrintString tmp;
        JsonTools::printToEscaped(tmp, value);
        return tmp;
    }
        break;
    case JsonBaseReader::JSON_TYPE_STRING: {
        PrintString tmp;
        tmp.write('"');
        JsonTools::printToEscaped(tmp, value);
        tmp.write('"');
        return tmp;
    }
    default:
        break;
    }
    return F("<invalid type>");
}

uint8_t JsonVar::getNumberType(const char *value)
{
    int decimalPlaces; // decimal places. > 0 means float, otherwise integer
    uint8_t result = 0;
    const char *ptr = value;
    // -?
    if (*ptr == '-') {
        result |= NEGATIVE;
        ptr++;
    }
    // (?=[1-9] | 0(?!\d))
    if (!((*ptr != '0' && isdigit(*ptr)) || (*ptr == '0' && !isdigit(*(ptr + 1))))) {
        return NumberType_t::INVALID;
    }
    if (*ptr++ == '0') {
        decimalPlaces = INT_MIN;
    }
    else {
        decimalPlaces = 0;
        // \d+
        while (isdigit(*ptr)) {
            if (*ptr == '0') {
                decimalPlaces--;
            } else {
                decimalPlaces = 0;
            }
            ptr++;
        }
    }
    // (
    // \\.
    if (*ptr == '.') {
        ptr++;
        // \d+
        if (!isdigit(*ptr)) {
            return NumberType_t::INVALID;
        }
        int pos = 0;
        while (isdigit(*ptr)) {
            pos++;
            if (*ptr != '0') {
                decimalPlaces = pos;
            }
            ptr++;
        }
    }
    // )?

    // (
    // [eE]
    if (tolower(*ptr) == 'e') {
        ptr++;
        // [+-]?
        auto exponentStart = ptr;
        if (*ptr == '+') {
            ptr++;
            exponentStart++;
        } else if (*ptr == '-') {
            ptr++;
        }
        // \d+
        if (!isdigit(*ptr++)) {
            return false;
        }
        while (isdigit(*ptr)) {
            ptr++;
        }
        result |= NumberType_t::EXPONENT;
        decimalPlaces -= strtol(exponentStart, nullptr, 10); // move decimal places
    }
    // )?
    if (*ptr) {
        return NumberType_t::INVALID;
    }

    // there is no decimal places left
    result |= (decimalPlaces > 0) ? NumberType_t::FLOAT : NumberType_t::INT;
    return result;
}

double JsonVar::getDouble(const char *value)
{
    char *endPtr = nullptr;
    auto result = strtod(value, &endPtr);
    if (*endPtr++ == 'e') {
        int exponent = (int)strtol(endPtr, nullptr, 10);
        while (exponent-- > 0) {
            result *= 10.0;
        }
        while (exponent++ < 0) {
            result /= 10.0;
        }
    }
    return result;
}

long JsonVar::getInt(const char *value, bool _unsigned)
{
    char *endPtr;
    auto result = _unsigned ? (long)strtoul(value, &endPtr, 10) : strtol(value, &endPtr, 10);
    unsigned long fraction = 0;
    unsigned int decimalPlaces = 0;

    // check if it has a fraction
    if (*endPtr == '.') {
        endPtr++;
        auto ptr = endPtr;
        fraction = strtoul(ptr, &endPtr, 10);
        if (fraction) {
            decimalPlaces = endPtr - ptr;
            while (fraction && fraction % 10 == 0) { // remove trailing 0s
                fraction /= 10;
                decimalPlaces--;
            }
        }
    }

    // check if it has an exponent
    if (tolower(*endPtr++) == 'e') {
        int exponent = (int)strtol(endPtr, nullptr, 10);
        if (exponent > 0) {
            if (fraction) {
                while (exponent--) {
                    result *= 10; // move decimal places
                    if (decimalPlaces) {
                        decimalPlaces--; // decrease counter before moving decimal places of the fraction
                    }
                    else {
                        fraction *= 10;
                    }
                }
                result += fraction;
                // decimal places must be 0 at this point, otherwise it is an invalid integer
            }
            else {
                while (exponent--) {
                    result *= 10; // move decimal places
                }
            }
        }
        else {
            // fraction must not exist here, otherwise it is an invalid integer
            while (exponent++) {
                result /= 10; // move decimal places
            }
        }
    }
    else {
        // fraction must not exist here, otherwise it is an invalid integer
    }
    return result;
}
