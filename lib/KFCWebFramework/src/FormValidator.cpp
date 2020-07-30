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

FormValidator::~FormValidator() 
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
    return (_enabled && !_field->getForm().isValid());
}

void FormValidator::setEnabled(bool value)
{
    _enabled = value;
}
