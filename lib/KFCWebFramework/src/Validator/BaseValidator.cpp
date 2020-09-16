/**
* Author: sascha_lammers@gmx.de
*/

#include "Validator/BaseValidator.h"
#include "Field/BaseField.h"
#include "Form/BaseForm.h"
#include "Form/Form.hpp"

#include "Utility/Debug.h"

using namespace FormUI::Validator;

BaseValidator::BaseValidator() : _field(nullptr)
{
}

BaseValidator::BaseValidator(const String &message) : _field(nullptr), _message(message)
{
}

BaseValidator::BaseValidator(const String &message, const __FlashStringHelper *defaultMessage) :
    BaseValidator(message.length() == 0 ? String(defaultMessage) : message)
{
}

void BaseValidator::setField(Field::BaseField *field)
{
    _field = field;
}

::FormUI::Field::BaseField &BaseValidator::getField()
{
    return *_field;
}

String BaseValidator::getMessage()
{
    return _message;
}

bool BaseValidator::validate()
{
    return true;
}
