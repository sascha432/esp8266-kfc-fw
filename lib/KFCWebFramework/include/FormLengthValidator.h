/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#undef _min
#undef _max

class FormLengthValidator : public FormValidator {
public:
    FormLengthValidator(size_t min, size_t max, bool allowEmpty = false) : FormValidator(FSPGM(FormLengthValidator_default_message)), _min(min), _max(max), _allowEmpty(allowEmpty) {
    }
    FormLengthValidator(const String &message, size_t min, size_t max, bool allowEmpty = false) : FormValidator(message.length() == 0 ? String(FSPGM(FormLengthValidator_default_message)) : message), _min(min), _max(max), _allowEmpty(allowEmpty) {
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            size_t len = getField().getValue().length();
            auto result = (len >= _min && len <= _max);
            if (!result && _allowEmpty) {
                String tmp = getField().getValue();
                tmp.trim();
                if (tmp.length() == 0) {
                    getField().setValue(tmp);
                    return true;
                }
            }
            return result;
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
    bool _allowEmpty;
};

