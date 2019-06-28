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
    virtual ~FormValidator();

    void setField(FormField *field);
    FormField *getField();

    virtual String getMessage();

    virtual bool validate();
    void setValidateIfValid(bool validateIfValid);

private:
    FormField *_field;
    String _message;
    bool _validateIfValid;
};

