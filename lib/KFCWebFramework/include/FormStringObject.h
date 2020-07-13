/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormField.h"

class FormStringObject : public FormField {
public:
    typedef std::function<bool(const String &strValue, FormField &field, bool store)> SetterCallback_t;

    FormStringObject(const String &name, const String &str, SetterCallback_t setter = nullptr, FormField::InputFieldType type = FormField::InputFieldType::TEXT) : FormField(name, str, type), _setter(setter), _str(nullptr) {
    }
    FormStringObject(const String &name, String *str, FormField::InputFieldType type = FormField::InputFieldType::TEXT) : FormField(name, *str, type), _setter(nullptr), _str(str) {
    }

    virtual void copyValue() {
        if (_setter) {
            _setter(getValue(), *this, true);
        }
        else if (_str) {
            *_str = getValue();
        }
    }

private:
    SetterCallback_t _setter;
    String *_str;
};

