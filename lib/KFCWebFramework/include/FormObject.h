/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

template <class C>
class FormObject : public FormField {
public:
    FormObject(const String &name, C obj, FormField::FieldType_t type = FormField::INPUT_TEXT) : FormField(name, obj.toString(), type), _obj(obj) {
    }

    virtual void copyValue() {
        _obj.fromString(getValue());
    }

private:
    C _obj;
};

