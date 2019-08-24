/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonArray.h"
#include "JsonObject.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

JsonUnnamedArray & JsonArrayMethods::addArray(size_t reserve)
{
    return reinterpret_cast<JsonUnnamedArray &>(add(_debug_new JsonUnnamedArray(reserve)));
}

JsonUnnamedObject & JsonArrayMethods::addObject(size_t reserve)
{
    return reinterpret_cast<JsonUnnamedObject &>(add(_debug_new JsonUnnamedObject(reserve)));
}
