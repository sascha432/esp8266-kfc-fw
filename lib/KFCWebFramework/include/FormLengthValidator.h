/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormLengthValidator : public FormValidator {
public:
    FormLengthValidator(size_t min, size_t max) : FormLengthValidator(FSPGM(FormLengthValidator_default_message), min, max) {
    }
    FormLengthValidator(const String &message, size_t min, size_t max) : FormValidator(message) {
        _min = min;
        _max = max;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            size_t len = getField().getValue().length();
            return (len >= _min && len <= _max);
        }
        return false;
    }

    virtual String getMessage() override {
        String message = FormValidator::getMessage();
        message.replace(FSPGM(FormValidator_min_macro), String(_min));
        message.replace(FSPGM(FormValidator_max_macro), String(_max));
        return message;
    }

private:
    size_t _min;
    size_t _max;
};

