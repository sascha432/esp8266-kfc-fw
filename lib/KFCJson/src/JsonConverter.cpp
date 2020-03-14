/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonConverter.h"
#include "JsonArray.h"
#include "JsonObject.h"

JsonConverter::JsonConverter(Stream &stream) : JsonBaseReader(stream), _current(nullptr), _ignoreInvalid(true), _filtered(0)
{
}

bool JsonConverter::beginObject(bool isArray)
{
    AbstractJsonValue *value;
    if (isArray) {
        if (_current && _current->hasChildName()) {
            value = new JsonArray(getKey());
        }
        else {
            value = new JsonUnnamedArray();
        }
    }
    else {
        if (_current && _current->hasChildName()) {
            value = new JsonObject(getKey());
        }
        else {
            value = new JsonUnnamedObject();
        }
    }
    if (_current) {
        _current->add(value);
        _stack.push_back(_current);
        _current = value;
    }
    else {
        _stack.push_back(value);
        _current = value;
    }
    return true;
}

bool JsonConverter::endObject()
{
    if (_stack.size()) {
        _current = _stack.back();
        _stack.pop_back();
        return true;
    }
    else {
        return false;
    }
}

bool JsonConverter::processElement()
{
    if (getType() == JSON_TYPE_INVALID) {
        return _ignoreInvalid;
    }
    if (_filter.size()) {
        auto str = getPath(false);
        auto iterator = std::find(_filter.begin(), _filter.end(), str);
        if (iterator == _filter.end()) {
            return true;
        }
    }
    AbstractJsonValue *value = nullptr;
    if (_current->hasChildName()) {
        switch (getType()) {
        case JSON_TYPE_BOOLEAN:
            value = new JsonNamedVariant<bool>(_keyStr, _valueStr.length() == 4);
            break;
        case JSON_TYPE_NUMBER:
        case JSON_TYPE_FLOAT:
            value = new JsonNamedVariant<JsonNumber>(_keyStr, JsonNumber(_valueStr));
            break;
        case JSON_TYPE_INT:
            value = new JsonNamedVariant<long>(_keyStr, _valueStr.toInt());
            break;
        case JSON_TYPE_NULL:
            value = new JsonNamedVariant<std::nullptr_t>(_keyStr, nullptr);
            break;
        case JSON_TYPE_STRING:
            value = new JsonNamedVariant<String>(_keyStr, _valueStr);
            break;
        default:
            break;
        }
    }
    else {
        switch (getType()) {
        case JSON_TYPE_BOOLEAN:
            value = new JsonUnnamedVariant<bool>(_valueStr.length() == 4);
            break;
        case JSON_TYPE_NUMBER:
        case JSON_TYPE_FLOAT:
            value = new JsonUnnamedVariant<JsonNumber>(JsonNumber(_valueStr));
            break;
        case JSON_TYPE_INT:
            value = new JsonUnnamedVariant<long>(_valueStr.toInt());
            break;
        case JSON_TYPE_NULL:
            value = new JsonUnnamedVariant<std::nullptr_t>(nullptr);
            break;
        case JSON_TYPE_STRING:
            value = new JsonUnnamedVariant<String>(_valueStr);
            break;
        default:
            break;
        }
    }
    _current->add(value);
    return true;
}

bool JsonConverter::recoverableError(JsonErrorEnum_t errorType)
{
    return _ignoreInvalid;
}

AbstractJsonValue *JsonConverter::getRoot() const
{
    if (_stack.size()) {
        return _stack.front();
    }
    return _current; // points to the root object or null
}

void JsonConverter::setIgnoreInvalid(bool ingnore)
{
    _ignoreInvalid = ingnore;
}

void JsonConverter::addFilter(const JsonString &str)
{
    _filter.push_back(str);
    _filtered = ~0;
}
