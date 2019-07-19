/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormField;

class FormError {
public:
    FormError();
    FormError(const String &message);
    FormError(FormField *field, const String &message);

    const String &getName() const;
    const String &getMessage() const;
    const bool is(FormField *field) const;

private:
    FormField *_field;
    String _message;
};
