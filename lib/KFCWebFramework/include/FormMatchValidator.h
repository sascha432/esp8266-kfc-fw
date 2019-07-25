/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormMatchValidator : public FormValidator {
public:
    typedef std::function<bool(FormField &field)> Callback_t;

    FormMatchValidator(const String &message, Callback_t callback) : FormValidator(message) {
        _callback = callback;
    }

    virtual bool validate() override {
        return FormValidator::validate() && _callback(getField());
    }

private:
    Callback_t _callback;
};
