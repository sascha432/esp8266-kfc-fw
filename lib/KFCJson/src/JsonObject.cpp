/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonObject.h"
#include "JsonArray.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

JsonArray & JsonObjectMethods::addArray(const JsonString & name, size_t reserve) {
    return reinterpret_cast<JsonArray &>(add(_debug_new JsonArray(name, reserve)));
}

JsonObject & JsonObjectMethods::addObject(const JsonString & name, size_t reserve) {
    return reinterpret_cast<JsonObject &>(add(_debug_new JsonObject(name, reserve)));
}

AbstractJsonValue & JsonObjectMethods::replace(const JsonString & name, AbstractJsonValue * value) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            auto &oldValue = *iterator;
            std::swap(oldValue, value);
            delete value;
            return *oldValue;
        }
    }
    return add(name, value);
}

AbstractJsonValue * JsonObjectMethods::find(const String & name) {
    return find(name.c_str());
}

AbstractJsonValue * JsonObjectMethods::find(const JsonString & name) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            return *iterator;
        }
    }
    return nullptr;
}

AbstractJsonValue * JsonObjectMethods::find(const __FlashStringHelper * name) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            return *iterator;
        }
    }
    return nullptr;
}

AbstractJsonValue * JsonObjectMethods::find(const char * name) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            return *iterator;
        }
    }
    return nullptr;
}

JsonUnnamedObject::JsonUnnamedObject(size_t reserve) : JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>(nullptr, reserve) {
}

size_t JsonUnnamedObject::printTo(Print & output) const {
    return output.write('{') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write('}');
}

AbstractJsonValue::JsonVariantEnum_t JsonUnnamedObject::getType() const {
    return AbstractJsonValue::JsonVariantEnum_t::JSON_UNNAMED_OBJECT;
}

bool JsonUnnamedObject::hasChildName() const {
    return true;
}

AbstractJsonValue & JsonUnnamedObject::add(AbstractJsonValue * value) {
    _getValue().push_back(value);
    return *_getValue().back();
}

AbstractJsonValue::JsonVariantVector * JsonUnnamedObject::getVector() {
    return &_getValue();
}

JsonObject::JsonObject(const JsonString & name, size_t reserve) : JsonNamedVariant<AbstractJsonValue::JsonVariantVector>(name, nullptr, reserve) {
}

size_t JsonObject::printTo(Print & output) const {
    return JsonNamedVariant<AbstractJsonValue::JsonVariantVector>::_printName(output) + output.write('{') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write('}');
}

AbstractJsonValue::JsonVariantEnum_t JsonObject::getType() const {
    return AbstractJsonValue::JsonVariantEnum_t::JSON_OBJECT;
}

bool JsonObject::hasChildName() const {
    return true;
}

AbstractJsonValue & JsonObject::add(AbstractJsonValue * value) {
    _getValue().push_back(value);
    return *_getValue().back();
}

AbstractJsonValue::JsonVariantVector * JsonObject::getVector() {
    return &_getValue();
}
