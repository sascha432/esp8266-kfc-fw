/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormField;

class FormValidator {
public:
    FormValidator();
    FormValidator(const String &message);
    FormValidator(const String &message, const __FlashStringHelper *defaultMessage);

    void setField(FormField *field);
    FormField &getField();

    virtual String getMessage();

    virtual bool validate();

private:
    FormField *_field;
    String _message;
};
