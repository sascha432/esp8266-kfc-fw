/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormMatchValidator : public FormValidator {
public:
    FormMatchValidator(const String &message, std::function<bool(FormField *field)> validatorcb) : FormValidator(message) {
        _validatorcb = validatorcb;
    }

    virtual bool validate() override {
        return FormValidator::validate() && _validatorcb(getField());
    }

private:
    std::function <bool(FormField *field)> _validatorcb;
};
