/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <misc.h>
#include "JsonValue.h"
#include "JsonString.h"
#include "JsonNumber.h"

template <class T>
class JsonUnnamedVariant : public AbstractJsonValue {
public:
    JsonUnnamedVariant(const __FlashStringHelper *value) : _value(value) {}
    JsonUnnamedVariant(const char *value) : _value(value) {}
    JsonUnnamedVariant(const JsonNumber &value) : _value(value) {}
    JsonUnnamedVariant(JsonNumber &&value) : _value(std::move(value)) {}
    JsonUnnamedVariant(const JsonVar &value) : _value(value) {}
    JsonUnnamedVariant(const JsonString &value) : _value(value) {}
    JsonUnnamedVariant(JsonString &&value) : _value(std::move(value)) {}
    JsonUnnamedVariant(const String &value) : _value(value) {}
    JsonUnnamedVariant(String &&value) : _value(std::move(value)) {}
    JsonUnnamedVariant(bool value) : _value(value) {}
    JsonUnnamedVariant(std::nullptr_t value) : _value(value) {}
    JsonUnnamedVariant(uint32_t value) : _value(value) {}
    JsonUnnamedVariant(int32_t value) : _value(value) {}
    JsonUnnamedVariant(uint64_t value) : _value(value) {}
    JsonUnnamedVariant(int64_t value) : _value(value) {}
    JsonUnnamedVariant(double value) : _value(value) {}
    virtual ~JsonUnnamedVariant() {
        _destroy(_value);
    }
protected:
    JsonUnnamedVariant(AbstractJsonValue::JsonVariantVector *value, size_t reserve = 0) { // we do not want to create a vector and copy it, value must be nullptr
        _value.reserve(reserve);
    }

public:
    virtual size_t printTo(Print &output) const {
        return _printTo(output, _value);
    }
    virtual size_t length() const {
        return _length(_value);
    }

    virtual JsonVariantEnum_t getType() const {
        return JsonVariantEnum_t::JSON_UNNAMED_VARIANT;
    }

    inline T &value() {
        return _value;
    }

protected:
    size_t _printTo(Print &output, const __FlashStringHelper *value) const {
        JsonTools::Utf8Buffer buffer;
        return output.write('"') + JsonTools::printToEscaped(output, value, &buffer) + output.write('"');
    }
    size_t _printTo(Print &output, const char *value) const {
        JsonTools::Utf8Buffer buffer;
        return output.write('"') + JsonTools::printToEscaped(output, value, strlen(value), &buffer) + output.write('"');
    }
    size_t _printTo(Print &output, const JsonVar &value) const {
        return output.print(JsonVar::formatValue(value.getValue(), value.getType()));
    }
    size_t _printTo(Print &output, const JsonNumber &value) const {
        return value.printTo(output);
    }
    size_t _printTo(Print &output, const JsonString &value) const {
        JsonTools::Utf8Buffer buffer;
        return output.write('"') + JsonTools::printToEscaped(output, value, &buffer) + output.write('"');
    }
    size_t _printTo(Print &output, const String &value) const {
        JsonTools::Utf8Buffer buffer;
        return output.write('"') + JsonTools::printToEscaped(output, value, &buffer) + output.write('"');
    }
    size_t _printTo(Print &output, bool value) const {
        return output.print(value ? FSPGM(true) : FSPGM(false));
    }
    size_t _printTo(Print &output, std::nullptr_t value) const {
        return output.print(FSPGM(null));
    }
    size_t _printTo(Print &output, uint32_t value) const {
        return output.print(value);
    }
    size_t _printTo(Print &output, int32_t value) const {
        return output.print(value);
    }
    size_t _printTo(Print &output, uint64_t value) const {
        return output.print(value);
    }
    size_t _printTo(Print &output, int64_t value) const {
        return output.print(value);
    }
    size_t _printTo(Print &output, double value) const {
        return printTrimmedDouble(&output, value);
    }
    size_t _printTo(Print &output, const AbstractJsonValue::JsonVariantVector &value) const {
        size_t length = 0;
        auto iter = value.begin();
        if (iter != value.end()) {
            length += (*iter)->printTo(output);
            while (++iter != value.end()) {
                length += output.print(',');
                length += (*iter)->printTo(output);
            }
        }
        return length;
    }

    size_t _length(const __FlashStringHelper *value) const {
        JsonTools::Utf8Buffer buffer;
        return JsonTools::lengthEscaped(value, &buffer) + 2;
    }
    size_t _length(const char *value) const {
        JsonTools::Utf8Buffer buffer;
        return JsonTools::lengthEscaped(value, strlen(value), &buffer) + 2;
    }
    size_t _length(const JsonVar &value) const {
        return JsonTools::lengthEscaped(JsonVar::formatValue(value.getValue(), value.getType()));
    }
    size_t _length(const JsonString &value) const {
        JsonTools::Utf8Buffer buffer;
        return JsonTools::lengthEscaped(value, &buffer) + 2;
    }
    size_t _length(const JsonNumber &value) const {
        return value.length();
    }
    size_t _length(const String &value) const {
        JsonTools::Utf8Buffer buffer;
        return JsonTools::lengthEscaped(value, &buffer) + 2;
    }
    size_t _length(bool value) const {
        return value ? 4 : 5;
    }
    size_t _length(std::nullptr_t value) const {
        return 4;
    }
    size_t _length(uint32_t value) const {
        return snprintf_P(nullptr, 0, PSTR("%u"), value);
    }
    size_t _length(int32_t value) const {
        return snprintf_P(nullptr, 0, PSTR("%d"), value);
    }
    size_t _length(uint64_t value) const {
        char buffer[std::numeric_limits<decltype(value)>::digits10 + 2];
        #if ESP8266
            auto str = ulltoa(value, buffer, sizeof(buffer), 10);
            return str ? &buffer[sizeof(buffer) - 1] - str : 0;
        #else
            return snprintf_P(buffer, sizeof(buffer), PSTR("%llu"), value);
        #endif
    }
    size_t _length(int64_t value) const {
        char buffer[std::numeric_limits<decltype(value)>::digits10 + 2];
        #if ESP8266
            auto str = lltoa(value, buffer, sizeof(buffer), 10);
            return str ? &buffer[sizeof(buffer) - 1] - str : 0;
        #else
            return snprintf_P(buffer, sizeof(buffer), PSTR("%lld"), value);
        #endif
    }
    size_t _length(double value) const {
        return printTrimmedDouble(nullptr, value);
    }
    size_t _length(const AbstractJsonValue::JsonVariantVector &value) const {
        size_t length = 2;
        if (value.size() > 1) {
            length += value.size() - 1;
        }
        for (auto variant : value) {
            length += variant->length();
        }
        return length;
    }

    inline void _destroy(AbstractJsonValue::JsonVariantVector &value) {
        for (auto variant : value) {
            delete variant;
        }
    }
    template <class R>
    inline void _destroy(R value) {
    }

protected:
    inline T &_getValue() {
        return _value;
    }

private:
    T _value;
};

template <class T>
class JsonNamedVariant : public JsonUnnamedVariant<T> {
public:
    JsonNamedVariant(const JsonString &name, const __FlashStringHelper *value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, const char *value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, const JsonVar &value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, const JsonString &value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, JsonString &&value) : JsonUnnamedVariant<T>(std::move(value)), _name(name) {}
    JsonNamedVariant(const JsonString &name, const JsonNumber &value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, JsonNumber &&value) : JsonUnnamedVariant<T>(std::move(value)), _name(name) {}
    JsonNamedVariant(const JsonString &name, const String &value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, String &&value) : JsonUnnamedVariant<T>(std::move(value)), _name(name) {}
    JsonNamedVariant(const JsonString &name, bool value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, std::nullptr_t value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, uint32_t value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, int32_t value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, uint64_t value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, int64_t value) : JsonUnnamedVariant<T>(value), _name(name) {}
    JsonNamedVariant(const JsonString &name, double value) : JsonUnnamedVariant<T>(value), _name(name) {}
protected:
    JsonNamedVariant(const JsonString &name, AbstractJsonValue::JsonVariantVector *value, size_t reserve = 0) : JsonUnnamedVariant<T>(value, reserve), _name(name) {
    }

public:
    virtual size_t printTo(Print &output) const {
        return _printName(output) + JsonUnnamedVariant<T>::printTo(output);
    }
    virtual size_t length() const {
        return _nameLength() + JsonUnnamedVariant<T>::length();
    }

    virtual AbstractJsonValue::JsonVariantEnum_t getType() const {
        return AbstractJsonValue::JsonVariantEnum_t::JSON_VARIANT;
    }

    inline const JsonString &name() const {
        return _name;
    }

protected:
    size_t _printName(Print &output) const {
        JsonTools::Utf8Buffer buffer;
        return output.write('"') + JsonTools::printToEscaped(output, _name, &buffer) + output.write('"') + output.write(':');
    }
    size_t _nameLength() const {
        JsonTools::Utf8Buffer buffer;
        return JsonTools::lengthEscaped(_name, &buffer) + 3;
    }
    virtual const JsonString *getName() const {
        return &_name;
    }
    virtual bool hasName() const {
        return true;
    }

    JsonString _name;
};
