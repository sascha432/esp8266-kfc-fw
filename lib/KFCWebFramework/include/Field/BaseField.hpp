/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Field/BaseField.h"
#include "Form/BaseForm.h"

#include "Utility/Debug.h"

using namespace FormUI;

__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::setForm(Form::BaseForm *form)
{
    _form = form;
}

__KFC_FORMS_INLINE_METHOD__
Form::BaseForm &Field::BaseField::getForm() const
{
    return *_form;
}

__KFC_FORMS_INLINE_METHOD__
WebUI::Config &Field::BaseField::getWebUIConfig()
{
    return _form->getWebUIConfig();
}

__KFC_FORMS_INLINE_METHOD__
const String &Field::BaseField::getName() const
{
    return _name;
}

__KFC_FORMS_INLINE_METHOD__
const char *Field::BaseField::getNameForType() const
{
    auto ptr = _name.c_str();
    if (*ptr == '.' || *ptr == '#') {
        ptr++;
    }
    return ptr;
}

/*
* This method is called when the user submits a valid form. The validated data is stored
* in the value field as a string and can be retried using getValue()
**/

__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::copyValue()
{
    Field::BaseField::getValue();
}

__KFC_FORMS_INLINE_METHOD__
bool Field::BaseField::equals(Field::BaseField *field) const
{
    return _value.equals(field->getValue());
}

__KFC_FORMS_INLINE_METHOD__
bool Field::BaseField::hasChanged() const
{
    return _hasChanged;
}

__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::setChanged(bool hasChanged)
{
    _hasChanged = hasChanged;
}

__KFC_FORMS_INLINE_METHOD__
void Field::BaseField::setType(Type type)
{
    _type = type;
}

__KFC_FORMS_INLINE_METHOD__
Field::Type Field::BaseField::getType() const
{
    return _type;
}

#include <debug_helper_disable.h>
