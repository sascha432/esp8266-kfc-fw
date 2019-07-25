/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>

template <typename T>
class FormValue : public FormField {
public:
    typedef std::function<void(T, FormField &)> SetterCallback_t;

    FormValue(const String &name, T value, SetterCallback_t setter = nullptr, FormField::FieldType_t type = FormField::INPUT_SELECT) : FormField(name, String(value), type) {
        _value = nullptr;
        _setter = setter;
    }
    FormValue(const String &name, T *value, FormField::FieldType_t type = FormField::INPUT_SELECT) : FormField(name, String(*value), type) {
        _value = value;
        _setter = nullptr;
    }

    virtual void copyValue() {
        if (_value) {
            *_value = (T)getValue();
        } else if (_setter) {
            _setter((T)getValue(), *this);
        }
    }

    virtual bool setValue(const String &value) override {
        return FormField::setValue(String(value.toInt()));
    }
    T getValue() {
        return (T)FormField::getValue().toInt();
    }

    // these modify the value that was passed to FormValue, not the value of the form
    void _setValue(T value) {
        *_value = value;
    }
    T _getValue() {
        return *_value;
    }

private:
    T *_value;
    SetterCallback_t _setter;
};
