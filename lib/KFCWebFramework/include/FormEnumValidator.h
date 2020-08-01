/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Form.h"

template <typename T, size_t N>
class FormEnumValidator : public FormValidator {
public:
    typedef std::array<T, N> ValuesArray_t;

    FormEnumValidator(ValuesArray_t values) : FormEnumValidator(FSPGM(FormEnumValidator_default_message), values) {
    }
    FormEnumValidator(const String &message, ValuesArray_t values) : FormValidator(message) {
        _values = values;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            T tmp = (T)getField().getValue().toInt();
            for(auto value: _values) {
                if (value == tmp) {
                    return true;
                }
            }
        }
        return false;
    }

    virtual String getMessage() override {
        String message = FormValidator::getMessage();
        if (message.indexOf(FSPGM(FormValidator_allowed_macro)) != -1) {
            String allowed = "[";
            for(auto value = _values.begin(); value != _values.end(); ++value) {
                allowed += String((long)*value);
                if (value != _values.end()) {
                    allowed += "|";
                }
            }
            allowed += "]";
            message.replace(FSPGM(FormValidator_allowed_macro), allowed);
        }
        return message;
    }

private:
    ValuesArray_t _values;
};

