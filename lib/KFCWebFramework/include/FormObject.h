/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormField.h"

template <class C>
class FormObject : public FormField {
public:
    typedef std::function<bool(const C &value, FormField &field, bool store)> SetterCallback_t;

    FormObject(const String &name, C obj, SetterCallback_t setter = nullptr, FormField::InputFieldType type = FormField::InputFieldType::TEXT) : FormField(name, obj.toString(), type), _obj(obj), _setter(setter) {
    }
    FormObject(const String &name, C obj, FormField::InputFieldType type = FormField::InputFieldType::TEXT) : FormField(name, obj.toString(), type), _obj(obj), _setter(nullptr) {
    }

    virtual void copyValue() {
        _obj.fromString(getValue());
        if (_setter) {
            _setter(_obj, *this, true);
        }
    }

private:
    C _obj;
    SetterCallback_t _setter;
};

