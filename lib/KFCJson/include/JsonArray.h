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
        return add(_debug_new JsonUnnamedVariant<const __FlashStringHelper *>(value));
    }
    AbstractJsonValue &add(const char *value) {
        return add(_debug_new JsonUnnamedVariant<const char *>(value));
    }
    AbstractJsonValue &add(const JsonVar &value) {
        return add(_debug_new JsonUnnamedVariant<JsonVar>(value));
    }
    AbstractJsonValue &add(const JsonString &value) {
        if (value.isProgMem()) {
            return add(_debug_new JsonUnnamedVariant<const __FlashStringHelper *>(value.getFPStr()));
        }
        return add(_debug_new JsonUnnamedVariant<JsonString>(value));
    }
    AbstractJsonValue &add(const JsonNumber &value) {
        return add(_debug_new JsonUnnamedVariant<JsonNumber>(value));
    }
    AbstractJsonValue &add(const String &value) {
        return add(_debug_new JsonUnnamedVariant<JsonString>(value));
    }
    AbstractJsonValue &add(bool value) {
        return add(_debug_new JsonUnnamedVariant<bool>((bool)value));
    }
    AbstractJsonValue &add(std::nullptr_t value) {
        return add(_debug_new JsonUnnamedVariant<std::nullptr_t>((std::nullptr_t)value));
    }
    AbstractJsonValue &add(int value) {
        return add(_debug_new JsonUnnamedVariant<long>((long)value));
    }
    AbstractJsonValue &add(unsigned int value) {
        return add(_debug_new JsonUnnamedVariant<unsigned long>((unsigned long)value));
    }
    AbstractJsonValue &add(long value) {
        return add(_debug_new JsonUnnamedVariant<long>((long)value));
    }
    AbstractJsonValue &add(unsigned long value) {
        return add(_debug_new JsonUnnamedVariant<unsigned long>((unsigned long)value));
    }
    AbstractJsonValue &add(double value) {
        return add(_debug_new JsonUnnamedVariant<double>(value));
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

    JsonUnnamedArray(size_t reserve = 0) : JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>(nullptr, reserve) {
    }

    virtual size_t printTo(Print &output) const {
        return output.write('[') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write(']');
    }

    virtual JsonVariantEnum_t getType() const {
        return JsonVariantEnum_t::JSON_UNNAMED_ARRAY;
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

class JsonArray : public JsonNamedVariant<AbstractJsonValue::JsonVariantVector>, public JsonArrayMethods {
public:
    using JsonArrayMethods::JsonArrayMethods::add;
    using JsonArrayMethods::JsonArrayMethods::addArray;
    using JsonArrayMethods::JsonArrayMethods::addObject;
    using JsonArrayMethods::JsonArrayMethods::size;
    using JsonArrayMethods::JsonArrayMethods::elements;

    JsonArray(const JsonString &name, size_t reserve = 0) : JsonNamedVariant<AbstractJsonValue::JsonVariantVector>(name, nullptr, reserve) {
    }

    virtual size_t printTo(Print &output) const {
        return JsonNamedVariant<AbstractJsonValue::JsonVariantVector>::_printName(output) + output.write('[') + JsonUnnamedVariant<AbstractJsonValue::JsonVariantVector>::printTo(output) + output.write(']');
    }

    virtual JsonVariantEnum_t getType() const {
        return JsonVariantEnum_t::JSON_ARRAY;
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
