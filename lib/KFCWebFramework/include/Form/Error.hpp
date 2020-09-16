/**
* Author: sascha_lammers@gmx.de
*/

#include "Form/Error.h"
#include "Field/BaseField.h"
#include "Utility/Debug.h"

using namespace FormUI::Form;

__KFC_FORMS_INLINE_METHOD__
const String & Error::getName() const
{
    if (!_field) {
        return emptyString;
    }
    return _field->getName();
}

__KFC_FORMS_INLINE_METHOD__
const String & Error::getMessage() const
{
    return _message;
}

__KFC_FORMS_INLINE_METHOD__
const bool Error::is(Field::BaseField &field) const
{
    return &field == _field;
}

#include <debug_helper_disable.h>
