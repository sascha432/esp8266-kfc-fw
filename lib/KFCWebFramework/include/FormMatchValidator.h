/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormMatchValidator : public FormValidator {
public:
    typedef std::function<bool(FormField &field)> Callback;

    FormMatchValidator(const String &message, Callback callback) : FormValidator(message) {
        _callback = callback;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            return _callback(getField());
        }
        return false;
    }

private:
    Callback _callback;
};
