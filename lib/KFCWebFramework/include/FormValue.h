/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <limits>

template <typename T>
class FormValue : public FormField {
public:
    // isSetter indicates to translate the variable to user format. return false if the getter callback is not supported
    typedef std::function<bool(T &, FormField&, bool isSetter)> GetterSetterCallback_t;

    FormValue(const String& name, T value, GetterSetterCallback_t callback = nullptr, FormField::FieldType_t type = FormField::INPUT_SELECT) : FormField(name), _value(nullptr), _callback(callback) {
        setType(type);
        _initValue(value);
    }
    FormValue(const String& name, T *value, GetterSetterCallback_t callback = nullptr, FormField::FieldType_t type = FormField::INPUT_SELECT) : FormField(name), _value(value), _callback(callback) {
        setType(type);
        _initValue(value);
    }
    FormValue(const String &name, T *value, FormField::FieldType_t type = FormField::INPUT_SELECT) : FormField(name), _value(value), _callback(nullptr) {
        setType(type);
        _initValue(value);
    }

    virtual void copyValue() {
        if (_callback) {
            if (_value) {
                *_value = (T)getValue(); // convert to T, the callback can use field.getValue() to read the actual string
                _callback((T &)*_value, *this, true);
            }
            else {
                auto tmp = getValue();
                _callback(tmp, *this, true);
            }
        }
        else if (_value) {
            *_value = (T)getValue();
        }
    }

    virtual bool setValue(const String &value) override {
        if (_callback) {
            auto tmp = (T)_stringToValue(value, (T)0);
            if (_callback(tmp, *this, true) && _callback(tmp, *this, false)) { // convert from user input and back to compare values
                return FormField::setValue(_valueToString(tmp));
            }
        }
        return FormField::setValue(_valueToString(_stringToValue(value, (T)0)));
    }

    T getValue() {
        return (T)_stringToValue(FormField::getValue(), (T)0);
    }

    // these modify the value that was passed to FormValue, not the value of the form
    void _setValue(T value) {
        *_value = value;
    }
    T _getValue() {
        return *_value;
    }

private:
    void _initValue(T value) {
        if (_callback) {
            T newValue = value;
            if (_callback(newValue, *this, false)) { // we have a callback, check if it supports getting the value
                initValue(_valueToString(newValue));
                return;
            }
        }
        initValue(_valueToString(value));
    }

    void _initValue(T *value) {
        _initValue(*value);
    }

    float _stringToValue(const String& str, float) {
        return str.toFloat();
    }

    double _stringToValue(const String& str, double) {
        return str.toFloat();
    }

    unsigned long _stringToValue(const String& str, unsigned long) {
        return str.toInt();
    }

    long _stringToValue(const String& str, long) {
        return str.toInt();
    }

    unsigned int _stringToValue(const String& str, unsigned int) {
        return str.toInt();
    }

    int _stringToValue(const String& str, int) {
        return str.toInt();
    }

    bool _stringToValue(const String& str, bool) {
        return str.toInt();
    }

    String _valueToString(float value) {
        return _valueToString((double)value, std::numeric_limits<float>::digits10);
    }

    String _valueToString(double value, int8_t digits = std::numeric_limits<double>::digits10) {
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

    String _valueToString(unsigned long value) {
        return String(value);
    }

    String _valueToString(long value) {
        return String(value);
    }

    String _valueToString(unsigned int value) {
        return String(value);
    }

    String _valueToString(int value) {
        return String(value);
    }

    String _valueToString(bool value) {
        return String(value);
    }

private:
    T *_value;
    GetterSetterCallback_t _callback;
};
