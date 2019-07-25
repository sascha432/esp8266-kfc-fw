/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

template <class C>
class FormObject : public FormField {
public:
    typedef std::function<void(const C&, FormField &)> SetterCallback_t;

    FormObject(const String &name, C obj, SetterCallback_t setter = nullptr, FormField::FieldType_t type = FormField::INPUT_TEXT) : FormField(name, obj.toString(), type), _obj(obj) {
        _setter = setter;
    }
    FormObject(const String &name, C obj, FormField::FieldType_t type = FormField::INPUT_TEXT) : FormField(name, obj.toString(), type), _obj(obj) {
        _setter = nullptr;
    }

    virtual void copyValue() {
        _obj.fromString(getValue());
        if (_setter) {
            _setter(_obj, *this);
        }
    }

private:
    C _obj;
    SetterCallback_t _setter;
};

