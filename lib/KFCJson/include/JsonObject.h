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
    AbstractJsonValue &add(const JsonString &name, JsonString &&value) {
        if (value.isProgMem()) {
            return add(_debug_new JsonNamedVariant<const __FlashStringHelper *>(name, value.getFPStr()));
        }
        return add(_debug_new JsonNamedVariant<JsonString>(name, std::move(value)));
    }
    AbstractJsonValue &add(const JsonString &name, const JsonNumber &value) {
        return add(_debug_new JsonNamedVariant<JsonNumber>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, JsonNumber &&value) {
        return add(_debug_new JsonNamedVariant<JsonNumber>(name, std::move(value)));
    }
    AbstractJsonValue &add(const JsonString &name, const String &value) {
        return add(_debug_new JsonNamedVariant<String>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, bool value) {
        return add(_debug_new JsonNamedVariant<bool>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, std::nullptr_t value) {
        return add(_debug_new JsonNamedVariant<std::nullptr_t>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, int value) {
        return add(_debug_new JsonNamedVariant<long>(name, (long)value));
    }
    AbstractJsonValue &add(const JsonString &name, unsigned int value) {
        return add(_debug_new JsonNamedVariant<unsigned long>(name, (unsigned long)value));
    }
    AbstractJsonValue &add(const JsonString &name, long value) {
        return add(_debug_new JsonNamedVariant<long>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, unsigned long value) {
        return add(_debug_new JsonNamedVariant<unsigned long>(name, value));
    }
    AbstractJsonValue &add(const JsonString &name, double value) {
        return add(_debug_new JsonNamedVariant<double>(name, value));
    }

    AbstractJsonValue &replace(const JsonString &name, const __FlashStringHelper *value) {
        return replace(name, new JsonNamedVariant<const __FlashStringHelper *>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, const char *value) {
        return replace(name, new JsonNamedVariant<const char *>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, const JsonString &value) {
        if (value.isProgMem()) {
            return replace(name, new JsonNamedVariant<const __FlashStringHelper *>(name, value.getFPStr()));
        }
        return replace(name, new JsonNamedVariant<JsonString>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, JsonString &&value) {
        if (value.isProgMem()) {
            return replace(name, new JsonNamedVariant<const __FlashStringHelper *>(name, value.getFPStr()));
        }
        return replace(name, new JsonNamedVariant<JsonString>(name, std::move(value)));
    }
    AbstractJsonValue &replace(const JsonString &name, const JsonNumber &value) {
        return replace(name, new JsonNamedVariant<JsonNumber>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, JsonNumber &&value) {
        return replace(name, new JsonNamedVariant<JsonNumber>(name, std::move(value)));
    }
    AbstractJsonValue &replace(const JsonString &name, const String &value) {
        return replace(name, new JsonNamedVariant<String>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, bool value) {
        return replace(name, new JsonNamedVariant<bool>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, std::nullptr_t value) {
        return replace(name, new JsonNamedVariant<std::nullptr_t>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, int value) {
        return replace(name, new JsonNamedVariant<long>(name, (long)value));
    }
    AbstractJsonValue &replace(const JsonString &name, unsigned int value) {
        return replace(name, new JsonNamedVariant<unsigned long>(name, (unsigned long)value));
    }
    AbstractJsonValue &replace(const JsonString &name, long value) {
        return replace(name, new JsonNamedVariant<long>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, unsigned long value) {
        return replace(name, new JsonNamedVariant<unsigned long>(name, value));
    }
    AbstractJsonValue &replace(const JsonString &name, double value) {
        return replace(name, new JsonNamedVariant<double>(name, value));
    }

    AbstractJsonValue *find(const String &name);
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

    AbstractJsonValue &replace(const JsonString &name, AbstractJsonValue *value);
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

    JsonUnnamedObject(size_t reserve = 0);

    virtual size_t printTo(Print &output) const;
    virtual AbstractJsonValue::JsonVariantEnum_t getType() const;
    virtual bool hasChildName() const;
    virtual AbstractJsonValue &add(AbstractJsonValue *value);

protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector();
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

    JsonObject(const JsonString &name, size_t reserve = 0);

    virtual size_t printTo(Print &output) const;
    virtual AbstractJsonValue::JsonVariantEnum_t getType() const;
    virtual bool hasChildName() const;
    virtual AbstractJsonValue &add(AbstractJsonValue *value);

protected:
    virtual AbstractJsonValue::JsonVariantVector *getVector();
};
