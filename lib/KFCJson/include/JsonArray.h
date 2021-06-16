/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// JsonUnnamedArray and JsonArray: specialized version of JsonUnnamedVariant and JsonNamedVariant

#include <Arduino_compat.h>
#include "JsonValue.h"
#include "JsonVariant.h"
#include "JsonString.h"
#include "JsonNumber.h"

class JsonUnnamedObject;
class JsonUnnamedArray;

class JsonArrayMethods {
public:
    AbstractJsonValue &add(const __FlashStringHelper *value) {
        return add(new JsonUnnamedVariant<const __FlashStringHelper *>(value));
    }
    AbstractJsonValue &add(const char *value) {
        return add(new JsonUnnamedVariant<const char *>(value));
    }
    AbstractJsonValue &add(const JsonVar &value) {
        return add(new JsonUnnamedVariant<JsonVar>(value));
    }
    AbstractJsonValue &add(const JsonString &value) {
        if (value.isProgMem()) {
            return add(new JsonUnnamedVariant<const __FlashStringHelper *>(value.getFPStr()));
        }
        return add(new JsonUnnamedVariant<JsonString>(value));
    }
    AbstractJsonValue &add(JsonString &&value) {
        if (value.isProgMem()) {
            return add(new JsonUnnamedVariant<const __FlashStringHelper *>(value.getFPStr()));
        }
        return add(new JsonUnnamedVariant<JsonString>(std::move(value)));
    }
    AbstractJsonValue &add(const JsonNumber &value) {
        return add(new JsonUnnamedVariant<JsonNumber>(value));
    }
    AbstractJsonValue &add(JsonNumber &&value) {
        return add(new JsonUnnamedVariant<JsonNumber>(std::move(value)));
    }
    AbstractJsonValue &add(const String &value) {
        return add(new JsonUnnamedVariant<JsonString>(value));
    }
    AbstractJsonValue &add(bool value) {
        return add(new JsonUnnamedVariant<bool>(value));
    }
    AbstractJsonValue &add(std::nullptr_t value) {
        return add(new JsonUnnamedVariant<std::nullptr_t>( value));
    }
    AbstractJsonValue &add(uint32_t value) {
        return add(new JsonUnnamedVariant<uint32_t>(value));
    }
    AbstractJsonValue &add(int32_t value) {
        return add(new JsonUnnamedVariant<int32_t>(value));
    }
    AbstractJsonValue &add(unsigned long value) {
        return add(new JsonUnnamedVariant<uint32_t>((uint32_t)value));
    }
    AbstractJsonValue &add(long value) {
        return add(new JsonUnnamedVariant<int32_t>((int32_t)value));
    }
    AbstractJsonValue &add(uint64_t value) {
        return add(new JsonUnnamedVariant<uint64_t>(value));
    }
    AbstractJsonValue &add(int64_t value) {
        return add(new JsonUnnamedVariant<int64_t>(value));
    }
    AbstractJsonValue &add(double value) {
        return add(new JsonUnnamedVariant<double>(value));
    }

    JsonUnnamedArray &addArray(size_t reserve = 0);
    JsonUnnamedObject &addObject(size_t reserve = 0);

    inline AbstractJsonValue::JsonVariantVector &elements() {
        return *getVector();
    }

    void clear() {
        for(auto variant : elements()) {
            delete variant;
        }
        elements().clear();
    }

    inline size_t size() {
        return getVector()->size();
    }

protected:
    virtual AbstractJsonValue &add(AbstractJsonValue *value) = 0;
    virtual AbstractJsonValue::JsonVariantVector *getVector() = 0;
};

class JsonUnnamedArray : public JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>, public JsonArrayMethods {
public:
    using JsonArrayMethods::JsonArrayMethods::add;
    using JsonArrayMethods::JsonArrayMethods::addArray;
    using JsonArrayMethods::JsonArrayMethods::addObject;
    using JsonArrayMethods::JsonArrayMethods::size;
    using JsonArrayMethods::JsonArrayMethods::elements;
    using JsonArrayMethods::JsonArrayMethods::clear;

    JsonUnnamedArray(size_t reserve = 0);

    virtual size_t printTo(Print &output) const;
    virtual AbstractJsonValue::JsonVariantEnum_t getType() const;
    virtual AbstractJsonValue &add(AbstractJsonValue *value);

protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector();
};

class JsonArray : public JsonNamedVariant<AbstractJsonValue::JsonVariantVector>, public JsonArrayMethods {
public:
    using JsonArrayMethods::JsonArrayMethods::add;
    using JsonArrayMethods::JsonArrayMethods::addArray;
    using JsonArrayMethods::JsonArrayMethods::addObject;
    using JsonArrayMethods::JsonArrayMethods::size;
    using JsonArrayMethods::JsonArrayMethods::elements;
    using JsonArrayMethods::JsonArrayMethods::clear;

    JsonArray(const JsonString &name, size_t reserve = 0);

    virtual size_t printTo(Print &output) const;
    virtual AbstractJsonValue::JsonVariantEnum_t getType() const;
    virtual AbstractJsonValue &add(AbstractJsonValue *value);

protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector();
};

