/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// JsonUnnamedObject and JsonObject: specialized version of JsonUnnamedVariant and JsonNamedVariant

#include <Arduino_compat.h>
#include "JsonValue.h"
#include "JsonVariant.h"
#include "JsonString.h"
#include "JsonNumber.h"

class JsonArray;
class JsonObject;

class JsonObjectMethods {
public:
    AbstractJsonValue &add(const JsonString &name, const __FlashStringHelper *value) {
        return add(_debug_new JsonNamedVariant<const __FlashStringHelper *>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, const char *value) {
        return add(_debug_new JsonNamedVariant<const char *>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, const JsonVar &value) {
        return add(_debug_new JsonNamedVariant<JsonVar>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, const JsonString &value) {
        if (value.isProgMem()) {
            return add(_debug_new JsonNamedVariant<const __FlashStringHelper *>(name, value.getFPStr()));
        }
        return add(_debug_new JsonNamedVariant<JsonString>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, const JsonNumber &value) {
        return add(_debug_new JsonNamedVariant<JsonNumber>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, const String &value) {
        return add(_debug_new JsonNamedVariant<String>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, bool value) {
        return add(_debug_new JsonNamedVariant<bool>(name, (bool)value));
    }
    AbstractJsonValue &add(const JsonString &name, std::nullptr_t value) {
        return add(_debug_new JsonNamedVariant<std::nullptr_t>(name, (std::nullptr_t)value));
    }
    AbstractJsonValue &add(const JsonString &name, int value) {
        return add(_debug_new JsonNamedVariant<long>(name, (long)value));
    }
    AbstractJsonValue &add(const JsonString &name, unsigned int value) {
        return add(_debug_new JsonNamedVariant<unsigned long>(name, (unsigned long)value));
    }
    AbstractJsonValue &add(const JsonString &name, long value) {
        return add(_debug_new JsonNamedVariant<long>(name, (long)value));
    }
    AbstractJsonValue &add(const JsonString &name, unsigned long value) {
        return add(_debug_new JsonNamedVariant<unsigned long>(name, (unsigned long)value));
    }
    AbstractJsonValue &add(const JsonString &name, double value) {
        return add(_debug_new JsonNamedVariant<double>(name, value));
    }

    AbstractJsonValue &replace(const JsonString &name, const __FlashStringHelper *value) {
        return replace<const __FlashStringHelper *>(name, value);
    }
    AbstractJsonValue &replace(const JsonString &name, const char *value) {
        return replace<const char *>(name, value);
    }
    AbstractJsonValue &replace(const JsonString &name, const JsonString &value) {
        if (value.isProgMem()) {
            return replace<const __FlashStringHelper *>(name, value.getFPStr());
        }
        return replace<JsonString>(name, value);
    }
    AbstractJsonValue &replace(const JsonString &name, const JsonNumber &value) {
        return replace<JsonNumber>(name, value);
    }
    AbstractJsonValue &replace(const JsonString &name, const String &value) {
        return replace<String>(name, value);
    }
    AbstractJsonValue &replace(const JsonString &name, bool value) {
        return replace<bool>(name, (bool)value);
    }
    AbstractJsonValue &replace(const JsonString &name, std::nullptr_t value) {
        return replace<std::nullptr_t>(name, (std::nullptr_t)value);
    }
    AbstractJsonValue &replace(const JsonString &name, int value) {
        return replace<long>(name, (long)value);
    }
    AbstractJsonValue &replace(const JsonString &name, unsigned int value) {
        return replace<unsigned long>(name, (unsigned long)value);
    }
    AbstractJsonValue &replace(const JsonString &name, long value) {
        return replace<long>(name, (long)value);
    }
    AbstractJsonValue &replace(const JsonString &name, unsigned long value) {
        return replace<unsigned long>(name, (unsigned long)value);
    }
    AbstractJsonValue &replace(const JsonString &name, double value) {
        return replace<double>(name, value);
    }

    AbstractJsonValue *find(const JsonString &name);
    AbstractJsonValue *find(const __FlashStringHelper *name);
    AbstractJsonValue *find(const char *name);

    JsonArray &addArray(const JsonString &name, size_t reserve = 0);
    JsonObject &addObject(const JsonString &name, size_t reserve = 0);

    inline AbstractJsonValue::JsonVariantVector &elements() {
        return *getVector();
    }

    inline size_t size() {
        return getVector()->size();
    }

protected:
    virtual AbstractJsonValue &add(AbstractJsonValue *value) = 0;
    virtual AbstractJsonValue::JsonVariantVector *getVector() = 0;

    template <class T>
    AbstractJsonValue &replace(const JsonString &name, T value) {
        auto vector = getVector();
        AbstractJsonValue *newValue = new JsonNamedVariant<T>(name, value);
        for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
            if (*(*iterator)->getName() == name) {
                auto &oldValue = *iterator;
                std::swap(oldValue, newValue);
                delete newValue;
                return *oldValue;
            }
        }
        return add(name, newValue);
    }
};

class JsonUnnamedObject : public JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>, JsonObjectMethods {
public:
    using JsonObjectMethods::JsonObjectMethods::find;
    using JsonObjectMethods::JsonObjectMethods::add;
    using JsonObjectMethods::JsonObjectMethods::replace;
    using JsonObjectMethods::JsonObjectMethods::addArray;
    using JsonObjectMethods::JsonObjectMethods::addObject;
    using JsonObjectMethods::JsonObjectMethods::size;
    using JsonObjectMethods::JsonObjectMethods::elements;

    JsonUnnamedObject(size_t reserve = 0) : JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>(nullptr, reserve) {
    }

    virtual size_t printTo(Print &output) const {
        return output.write('{') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write('}');
    }

    virtual JsonVariantEnum_t getType() const {
        return JsonVariantEnum_t::JSON_UNNAMED_OBJECT;
    }

    virtual bool hasChildName() const {
        return true;
    }

    virtual AbstractJsonValue &add(AbstractJsonValue *value) {
        _getValue().push_back(value);
        return *_getValue().back();
    }
protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector() {
        return &_getValue();
    }
};

class JsonObject : public JsonNamedVariant<AbstractJsonValue::JsonVariantVector>, public JsonObjectMethods {
public:
    using JsonObjectMethods::JsonObjectMethods::find;
    using JsonObjectMethods::JsonObjectMethods::add;
    using JsonObjectMethods::JsonObjectMethods::replace;
    using JsonObjectMethods::JsonObjectMethods::addArray;
    using JsonObjectMethods::JsonObjectMethods::addObject;
    using JsonObjectMethods::JsonObjectMethods::size;
    using JsonObjectMethods::JsonObjectMethods::elements;

    JsonObject(const JsonString &name, size_t reserve = 0) : JsonNamedVariant<AbstractJsonValue::JsonVariantVector>(name, nullptr, reserve) {
    }

    virtual size_t printTo(Print &output) const {
        return JsonNamedVariant<AbstractJsonValue::JsonVariantVector>::_printName(output) + output.write('{') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write('}');
    }

    virtual JsonVariantEnum_t getType() const {
        return JsonVariantEnum_t::JSON_OBJECT;
    }

    virtual bool hasChildName() const {
        return true;
    }

    virtual AbstractJsonValue &add(AbstractJsonValue *value) {
        _getValue().push_back(value);
        return *_getValue().back();
    }
protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector() {
        return &_getValue();
    }
};
