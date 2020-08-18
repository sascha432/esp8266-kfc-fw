/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonNumber.h"
#include "JsonVar.h"
#include "JsonTools.h"
#include <int64_to_string.h>

JsonNumber::JsonNumber(double value, uint8_t decimalPlaces) : JsonString()
{
    if (isnan(value) || isinf(value)) {
        invalidate();
    }
    else {
        char buf[std::numeric_limits<double>::digits + 1];
        int len = snprintf_P(buf, sizeof(buf), PSTR("%.*f"), decimalPlaces, value);
        _init(buf, len);
    }
}

JsonNumber::JsonNumber(uint32_t value) : JsonString()
{
    char buf[12];
    int len = snprintf_P(buf, sizeof(buf), PSTR("%u"), value);
    _init(buf, len);
}

JsonNumber::JsonNumber(int32_t value) : JsonString()
{
    char buf[12];
    int len = snprintf_P(buf, sizeof(buf), PSTR("%d"), value);
    _init(buf, len);
}

JsonNumber::JsonNumber(uint64_t value) : JsonString()
{
    char buf[kInt64ToStringBufferSize];
    char *str = ulltoa(value, buf);
    _init(str, &buf[kInt64ToStringBufferSize - 1] - str);
}

JsonNumber::JsonNumber(int64_t value) : JsonString()
{
    char buf[kInt64ToStringBufferSize];
    char *str = lltoa(value, buf);
    _init(str, &buf[kInt64ToStringBufferSize - 1] - str);
}

bool JsonNumber::validate()
{
    bool valid;
    if (isProgMem()) {
        String tmp = FPSTR(getPtr());
        valid = JsonVar::getNumberType(tmp.c_str()) != JsonVar::INVALID;
    }
    else {
        valid = JsonVar::getNumberType(getPtr()) != JsonVar::INVALID;
    }
    if (!valid) {
        invalidate();
    }
    return valid;
}

void JsonNumber::invalidate()
{
    _setType(STORED);
    strcpy_P(_raw, SPGM(null));
}
