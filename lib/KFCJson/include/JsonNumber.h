/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonString.h"

// can be used to store integer or float with "unlimited" precision, or use exponential E notation
// validate() checks the value and replaces it with 0 if invalid
class JsonNumber : public JsonString {
public:
    using JsonString::JsonString;

    bool validate();
    void invalidate();
};
