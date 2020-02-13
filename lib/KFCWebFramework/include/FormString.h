/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormField.h"

template <size_t size>
class FormString : public FormField {
public:
    FormString(const String &name, char *value, FormField::InputFieldType type = FormField::InputFieldType::TEXT) : FormField(name, value, type), _value(value) {
    }

    virtual void copyValue() {
        strncpy(_value, getValue().c_str(), size - 1)[size - 1] = 0;
    }

private:
    char *_value;
};
