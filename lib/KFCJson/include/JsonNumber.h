/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonString.h"

// can be used to store integer or float with "unlimited" precision, fixed decimal places, or using exponential E notation
// validate() checks the value and replaces it with 0 if invalid
class JsonNumber : public JsonString {
public:
    using JsonString::JsonString;

    JsonNumber(double value, uint8_t decimalPlaces);
    JsonNumber(uint32_t value);
    JsonNumber(int32_t value);
    JsonNumber(uint64_t value);
    JsonNumber(int64_t value);

    bool validate();
    void invalidate();
};
