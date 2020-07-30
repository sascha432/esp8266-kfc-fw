/**
* Author: sascha_lammers@gmx.de
*/

#include "FormValidator.h"
#include "FormField.h"
#include "Form.h"

FormValidator::FormValidator() : _field(nullptr), _enabled(true) 
{
}

FormValidator::FormValidator(const String &message) : _field(nullptr), _enabled(true), _message(message)
{
}

void FormValidator::setField(FormField * field) 
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
    if (_enabled) {
        if (_field->getForm().isValid()) {
            return true;
        }
    }
    return false;
}

void FormValidator::setEnabled(bool value)
{
    _enabled = value;
}
