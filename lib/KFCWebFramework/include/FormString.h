/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormField.h"

class FormCString : public FormField {
public:
    FormCString(const String &name, char *value, size_t maxSize, FormField::Type type = FormField::Type::TEXT) : FormField(name, value, type), _value(value), _size(maxSize) {
    }

    virtual void copyValue() {
        strncpy(_value, getValue().c_str(), _size - 1)[_size - 1] = 0;
    }

private:
    char *_value;
    size_t _size;
};
