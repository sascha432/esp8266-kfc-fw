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

#include <debug_helper_enable_mem.h>

JsonUnnamedArray & JsonArrayMethods::addArray(size_t reserve)
{
    return reinterpret_cast<JsonUnnamedArray &>(add(__LDBG_new(JsonUnnamedArray, reserve)));
}

JsonUnnamedObject & JsonArrayMethods::addObject(size_t reserve)
{
    return reinterpret_cast<JsonUnnamedObject &>(add(__LDBG_new(JsonUnnamedObject, reserve)));
}

JsonUnnamedArray::JsonUnnamedArray(size_t reserve) : JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>(nullptr, reserve)
{
}

size_t JsonUnnamedArray::printTo(Print & output) const
{
    return output.write('[') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write(']');
}

AbstractJsonValue::JsonVariantEnum_t JsonUnnamedArray::getType() const
{
    return AbstractJsonValue::JsonVariantEnum_t::JSON_UNNAMED_ARRAY;
}

AbstractJsonValue & JsonUnnamedArray::add(AbstractJsonValue * value)
{
    _getValue().push_back(value);
    return *_getValue().back();
}

AbstractJsonValue::JsonVariantVector * JsonUnnamedArray::getVector()
{
    return &_getValue();
}

JsonArray::JsonArray(const JsonString & name, size_t reserve) : JsonNamedVariant<AbstractJsonValue::JsonVariantVector>(name, nullptr, reserve)
{
}

size_t JsonArray::printTo(Print & output) const
{
    return JsonNamedVariant<AbstractJsonValue::JsonVariantVector>::_printName(output) + output.write('[') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write(']');
}

AbstractJsonValue::JsonVariantEnum_t JsonArray::getType() const
{
    return AbstractJsonValue::JsonVariantEnum_t::JSON_ARRAY;
}

AbstractJsonValue & JsonArray::add(AbstractJsonValue * value)
{
    _getValue().push_back(value);
    return *_getValue().back();
}

AbstractJsonValue::JsonVariantVector * JsonArray::getVector()
{
    return &_getValue();
}
