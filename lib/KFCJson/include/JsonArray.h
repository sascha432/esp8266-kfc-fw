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

#include <debug_helper_enable_mem.h>

class JsonUnnamedObject;
class JsonUnnamedArray;

class JsonArrayMethods {
public:
    AbstractJsonValue &add(const __FlashStringHelper *value) {
        return add(__LDBG_new(JsonUnnamedVariant<const __FlashStringHelper *>, value));
    }
    AbstractJsonValue &add(const char *value) {
        return add(__LDBG_new(JsonUnnamedVariant<const char *>, value));
    }
    AbstractJsonValue &add(const JsonVar &value) {
        return add(__LDBG_new(JsonUnnamedVariant<JsonVar>, value));
    }
    AbstractJsonValue &add(const JsonString &value) {
        if (value.isProgMem()) {
            return add(__LDBG_new(JsonUnnamedVariant<const __FlashStringHelper *>, value.getFPStr()));
        }
        return add(__LDBG_new(JsonUnnamedVariant<JsonString>, value));
    }
    AbstractJsonValue &add(JsonString &&value) {
        if (value.isProgMem()) {
            return add(__LDBG_new(JsonUnnamedVariant<const __FlashStringHelper *>, value.getFPStr()));
        }
        return add(__LDBG_new(JsonUnnamedVariant<JsonString>, std::move(value)));
    }
    AbstractJsonValue &add(const JsonNumber &value) {
        return add(__LDBG_new(JsonUnnamedVariant<JsonNumber>, value));
    }
    AbstractJsonValue &add(JsonNumber &&value) {
        return add(__LDBG_new(JsonUnnamedVariant<JsonNumber>, std::move(value)));
    }
    AbstractJsonValue &add(const String &value) {
        return add(__LDBG_new(JsonUnnamedVariant<JsonString>, value));
    }
    AbstractJsonValue &add(bool value) {
        return add(__LDBG_new(JsonUnnamedVariant<bool>, value));
    }
    AbstractJsonValue &add(std::nullptr_t value) {
        return add(__LDBG_new(JsonUnnamedVariant<std::nullptr_t>, value));
    }
    AbstractJsonValue &add(uint32_t value) {
        return add(__LDBG_new(JsonUnnamedVariant<uint32_t>, value));
    }
    AbstractJsonValue &add(int32_t value) {
        return add(__LDBG_new(JsonUnnamedVariant<int32_t>, value));
    }
    AbstractJsonValue &add(unsigned long value) {
        return add(__LDBG_new(JsonUnnamedVariant<uint32_t>, (uint32_t)value));
    }
    AbstractJsonValue &add(long value) {
        return add(__LDBG_new(JsonUnnamedVariant<int32_t>, (int32_t)value));
    }
    AbstractJsonValue &add(uint64_t value) {
        return add(__LDBG_new(JsonUnnamedVariant<uint64_t>, value));
    }
    AbstractJsonValue &add(int64_t value) {
        return add(__LDBG_new(JsonUnnamedVariant<int64_t>, value));
    }
    AbstractJsonValue &add(double value) {
        return add(__LDBG_new(JsonUnnamedVariant<double>, value));
    }

    JsonUnnamedArray &addArray(size_t reserve = 0);
    JsonUnnamedObject &addObject(size_t reserve = 0);

    inline AbstractJsonValue::JsonVariantVector &elements() {
        return *getVector();
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

    JsonArray(const JsonString &name, size_t reserve = 0);

    virtual size_t printTo(Print &output) const;
    virtual AbstractJsonValue::JsonVariantEnum_t getType() const;
    virtual AbstractJsonValue &add(AbstractJsonValue *value);

protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector();
};

#include <debug_helper_disable_mem.h>
