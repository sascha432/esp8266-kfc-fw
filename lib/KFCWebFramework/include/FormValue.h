/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <stl_ext/type_traits.h>
#include <limits>
#include <misc.h>
#include "FormField.h"

template <class Ta>
class FormValueBase : public FormField
{
public:
    using _VarType = typename std::conditional<std::is_reference<Ta>::value, typename std::remove_reference<Ta>::type, typename std::remove_pointer<Ta>::type>::type;
    using VarType = typename std::relaxed_underlying_type<_VarType>::type;

    FormValueBase(const String &name, const String &value = String(), Type type = Type::NONE) : FormField(name, value, type) {}
    FormValueBase(const String &name, Type type = Type::NONE) : FormField(name, String(), type) {}

protected:
    // Setter
    void _stringToValue(String &value, const String &str) const {
        value = str;
    }

    void _stringToValue(IPAddress &value, const String &str) const {
        value = (uint32_t)0;
        if (!value.fromString(str)) {
            value = (uint32_t)0;
        }
    }

    void _stringToValue(double &value, const String &str) const {
        value = str.toFloat();
    }

    void _stringToValue(float &value, const String &str) const {
        value = str.toFloat();
    }

    void _stringToValue(unsigned long &value, const String &str) const {
        value = str.toInt();
    }

    void _stringToValue(long &value, const String &str) const {
        value = str.toInt();
    }

    void _stringToValue(uint32_t &value, const String &str) const {
        value = str.toInt();
    }

    void _stringToValue(uint16_t &value, const String &str) const {
        value = (uint16_t)str.toInt();
    }

    void _stringToValue(uint8_t &value, const String &str) const {
        value = (uint8_t)str.toInt();
    }

    void _stringToValue(int32_t &value, const String &str) const {
        value = str.toInt();
    }

    void _stringToValue(int16_t &value, const String &str) const {
        value = (int16_t)str.toInt();
    }

    void _stringToValue(int8_t &value, const String &str) const {
        value = (int8_t)str.toInt();
    }

    void _stringToValue(bool &value, const String &str) const {
        value = (bool)str.toInt();
    }

protected:
    // Getter

    String _valueToString(const String &str) const {
        return str;
    }

    String _valueToString(const IPAddress &address) const {
        return address.toString();
    }

    String _valueToString(float value) const {
        return _valueToString((double)value, std::numeric_limits<float>::digits10);
    }

    String _valueToString(double value, int8_t digits = std::numeric_limits<double>::digits10) const {
        // calculate maximum precision and strip trailing zeros
        uint64_t tmp = (uint64_t)(value < 0 ? -value : value);
        while (tmp > 0) {
            digits--;
            tmp /= 10;
        }
        if (digits > 0) {
            String str = String(value, digits);
            while (str.charAt(str.length() - 1) == '0') {
                str.remove(str.length() - 1, 1);
            }
            if (str.charAt(str.length() - 1) == '.') {
                str += '0';
            }
            return str;
        }
        return String(value);
    }

    String _valueToString(unsigned long value) const {
        return String(value);
    }

    String _valueToString(long value) const {
        return String(value);
    }

    String _valueToString(uint32_t value) const {
        return String(value);
    }

    String _valueToString(int32_t value) const {
        return String(value);
    }

    String _valueToString(uint16_t value) const {
        return String(value);
    }

    String _valueToString(int16_t value) const {
        return String(value);
    }

    String _valueToString(uint8_t value) const {
        return String(value);
    }

    String _valueToString(int8_t value) const {
        return String(value);
    }

    String _valueToString(bool value) const {
        return String(value);
    }
};

template <typename Ta>
class FormValueCallback : public FormValueBase<Ta>
{
public:
    using Type = FormField::Type;
    using _VarType = typename FormValueBase<Ta>::_VarType;
    using VarType = typename FormValueBase<Ta>::VarType;
    using GetterSetterCallback = std::function<bool(_VarType &value, FormField &field, bool store)>;
    using Callback = GetterSetterCallback;
    using SetterCallback = std::function<void(_VarType &value, FormField &field)>;
    using FormValueBase<Ta>::_stringToValue;
    using FormValueBase<Ta>::_valueToString;

protected:
    GetterSetterCallback _callback;

public:
    FormValueCallback(const String &name, const VarType &value, GetterSetterCallback callback, Type type = Type::SELECT) :
        FormValueBase<Ta>(name, type),
        _callback(callback)
    {
        _initValue(static_cast<VarType>(value));
    }

    FormValueCallback(const String &name, const _VarType &value, SetterCallback callback, Type type = Type::SELECT) :
        FormValueBase<Ta>(name, type),
        _callback([callback](_VarType &value, FormField &field, bool store) {
            if (store) {
                callback(value, field);
            }
            return false;
        })
    {
        _initValue(static_cast<VarType>(value));
    }

    FormValueCallback(const String &name, GetterSetterCallback callback, Type type = Type::SELECT) :
        FormValueBase<Ta>(name, type),
        _callback(callback)
    {
        _initValue();
    }

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

    virtual void copyValue() override {
        VarType tmp;
        _stringToValue(tmp, FormField::getValue());
        _callback(reinterpret_cast<_VarType &>(tmp), *this, true);
    }

    virtual bool setValue(const String &value) override {
        VarType tmp;
        _stringToValue(tmp, value);
        if (!(_callback(reinterpret_cast<_VarType &>(tmp), *this, true) && _callback(reinterpret_cast<_VarType &>(tmp), *this, false))) { // convert from user input and back to compare values
            // callback failed
            _stringToValue(tmp, value);
        }
        return FormField::setValue(_valueToString(tmp));
    }

protected:
    void _initValue(VarType value = VarType())
    {
        VarType tmp;
        if (_callback(reinterpret_cast<_VarType &>(tmp), *this, false)) { // we have a callback, check if it supports getting the value
            // replace on success
            value = tmp;
        }
        FormField::initValue(_valueToString(value));
    }
};

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

template <typename Ta>
class FormValuePointer : public FormValueBase<Ta> {
public:
    using Type = FormField::Type;
    using _VarType = typename FormValueBase<Ta>::_VarType;
    using VarType = typename FormValueBase<Ta>::VarType;
    using FormValueBase<Ta>::_stringToValue;
    using FormValueBase<Ta>::_valueToString;

protected:
    VarType *__value;

public:
    FormValuePointer(const String &name, VarType *value, Type type = Type::SELECT) : FormValueBase<Ta>(name, type), __value(_initValue(value)) {
    }
    FormValuePointer(const String &name, VarType &value, Type type = Type::SELECT) : FormValueBase<Ta>(name, type), __value(_initValue(&value)) {
    }

    virtual void copyValue() override  {
        _stringToValue(*__value, FormField::getValue());
    }

    virtual bool setValue(const String &value) override {
        VarType tmp;
        _stringToValue(tmp, value);
        return FormField::setValue(_valueToString(tmp));
    }

    VarType getValue() const {
        VarType tmp;
        _stringToValue(tmp, FormField::getValue());
        return tmp;
    }

    // these modify the value that was passed to FormValue, not the value of the form
    void _setValue(VarType value) {
        *__value = value;
    }

    const VarType &_getValue() const {
        return *__value;
    }

protected:
    VarType *_initValue(VarType *value) {
        FormField::initValue(_valueToString(*value));
        return value;
    }
};
