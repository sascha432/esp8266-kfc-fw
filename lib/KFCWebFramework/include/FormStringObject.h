/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormField.h"

class FormStringObject : public FormField {
public:
    typedef std::function<bool(const String &strValue, FormField &field, bool store)> Callback;

    FormStringObject(const String &name, const String &str, Callback callback = nullptr, FormField::Type type = FormField::Type::TEXT) : FormField(name, str, type), _callback(callback), _str(nullptr) {
    }
    FormStringObject(const String &name, String &str, FormField::Type type = FormField::Type::TEXT) : FormField(name, str, type), _callback(nullptr), _str(&str) {
    }

    virtual void copyValue() {
        if (_callback) {
            _callback(getValue(), *this, true);
        }
        else if (_str) {
            *_str = getValue();
        }
    }

private:
    Callback _callback;
    String *_str;
};

