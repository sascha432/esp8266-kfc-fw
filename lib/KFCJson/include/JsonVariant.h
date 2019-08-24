/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "JsonValue.h"
#include "JsonString.h"
#include "JsonNumber.h"

template <class T>
class JsonUnnamedVariant : public AbstractJsonValue {
public:
    JsonUnnamedVariant(const __FlashStringHelper *value) : _value(value) {
    }
    JsonUnnamedVariant(const char *value) : _value(value) {
    }
    JsonUnnamedVariant(const JsonNumber &value) : _value(value) {
    }
    JsonUnnamedVariant(const JsonVar &value) : _value(value) {
    }
    JsonUnnamedVariant(const JsonString &value) : _value(value) {
    }
    JsonUnnamedVariant(const String &value) : _value(value) {
    }
    JsonUnnamedVariant(bool value) : _value(value) {
    }
    JsonUnnamedVariant(std::nullptr_t value) : _value(value) {
    }
    JsonUnnamedVariant(long value) : _value(value) {
    }
    JsonUnnamedVariant(unsigned long value) : _value(value) {
    }
    JsonUnnamedVariant(double value) : _value(value) {
    }
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
        return output.write('"') + JsonTools::printToEscaped(output, value) + output.write('"');
    }
    size_t _printTo(Print &output, const char *value) const {
        return output.write('"') + JsonTools::printToEscaped(output, value, strlen(value)) + output.write('"');
    }
    size_t _printTo(Print &output, const JsonNumber &value) const {
        auto ptr = value.getPtr();
        if (ptr) {
            if (value.isProgMem()) {
                return output.print(FPSTR(ptr));
            } else {
                return output.write((const uint8_t *)ptr, value.length());
            }
        }
        return 0;
    }
    size_t _printTo(Print &output, const JsonVar &value) const {
        return output.print(JsonVar::formatValue(value.getValue(), value.getType()));
    }
    size_t _printTo(Print &output, const JsonString &value) const {
        return output.write('"') + JsonTools::printToEscaped(output, value) + output.write('"');
    }
    size_t _printTo(Print &output, const String &value) const {
        return output.write('"') + JsonTools::printToEscaped(output, value) + output.write('"');
    }
    size_t _printTo(Print &output, bool value) const {
        return output.print(value ? FSPGM(true) : FSPGM(false));
    }
    size_t _printTo(Print &output, std::nullptr_t value) const {
        return output.print(FSPGM(null));
    }
    size_t _printTo(Print &output, long value) const {
        return output.print(value);
    }
    size_t _printTo(Print &output, unsigned long value) const {
        return output.print(value);
    }
    size_t _printTo(Print &output, double value) const {
        char buf[64];
        _printDouble(buf, (uint8_t)sizeof(buf), value);
        return output.print(buf);
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
        return JsonTools::lengthEscaped(value) + 2;
    }
    size_t _length(const char *value) const {
        return JsonTools::lengthEscaped(value, strlen(value)) + 2;
    }
    size_t _length(const JsonVar &value) const {
        return JsonTools::lengthEscaped(JsonVar::formatValue(value.getValue(), value.getType()));
    }
    size_t _length(const JsonString &value) const {
        return JsonTools::lengthEscaped(value) + 2;
    }
    size_t _length(const JsonNumber &value) const {
        return value.length();
    }
    size_t _length(const String &value) const {
        return JsonTools::lengthEscaped(value) + 2;
    }
    size_t _length(bool value) const {
        return value ? 4 : 5;
    }
    size_t _length(std::nullptr_t value) const {
        return 4;
    }
    size_t _length(long value) const {
        char buf[2];
        return snprintf_P(buf, 2, PSTR("%ld"), value);
    }
    size_t _length(unsigned long value) const {
        char buf[2];
        return snprintf_P(buf, 2, PSTR("%lu"), value);
    }
    size_t _length(double value) const {
        char buf[64];
        return _printDouble(buf, (uint8_t)sizeof(buf), value);
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
    T &_getValue() {
        return _value;
    }

private:
    // print double with maximum precision without trailing zeros, but at least one decimal place which can be 0
    size_t _printDouble(char *buf, uint8_t size, double value) const {
        if (isnan(value) || isinf(value)) {
            strcat_P(buf, SPGM(null));
            return 4;
        }
        uint8_t precision = std::numeric_limits<double>::digits10;
        auto number = value;
        if (number < 0) {
            number = -number;
        }
        while ((number = (number / 10)) > 1 && precision > 1) {
            precision--;
        }
        auto len = snprintf_P(buf, size, PSTR("%.*f"), precision, value);
        char *ptr;
        if (len < size) {
            ptr = strchr(buf, '.');
            if (ptr++) {
                char *endPtr = ptr + strlen(ptr);
                while(--endPtr > ptr && *endPtr == '0') {
                    *endPtr = 0;
                    len--;
                }
            }
        }
        return len;
    }

private:
    T _value;
};

template <class T>
class JsonNamedVariant : public JsonUnnamedVariant<T> {
public:

    JsonNamedVariant(const JsonString &name, const __FlashStringHelper *value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, const char *value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, const JsonVar &value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, const JsonString &value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, const JsonNumber &value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, const String &value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, bool value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, std::nullptr_t value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, long value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, unsigned long value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
    JsonNamedVariant(const JsonString &name, double value) : JsonUnnamedVariant<T>(value), _name(name) {
    }
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
        return output.write('"') + JsonTools::printToEscaped(output, _name) + output.write('"') + output.write(':');
    }
    size_t _nameLength() const {
        return JsonTools::lengthEscaped(_name) + 3;
    }
    virtual const JsonString *getName() const {
        return &_name;
    }
    virtual bool hasName() const {
        return true;
    }

    JsonString _name;
};

//class JsonVectorVariant {
//public:
//};
