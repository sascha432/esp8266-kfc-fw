/**
* Author: sascha_lammers@gmx.de
*/

#include "FormValidator.h"
#include "FormField.h"
#include "Form.h"

FormValidator::FormValidator() : _field(nullptr)
{
}

FormValidator::FormValidator(const String &message) : _field(nullptr), _message(message)
{
}

FormValidator::FormValidator(const String &message, const __FlashStringHelper *defaultMessage) : FormValidator((message.length() == 0) ? String(defaultMessage) : message)
{
}

void FormValidator::setField(FormField *field)
{
    _field = field;
}

FormField &FormValidator::getField()
{
    return *_field;
}

String FormValidator::getMessage()
{
    return _message;
}

bool FormValidator::validate()
{
    return true;
}
