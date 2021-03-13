/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Field/BaseField.h"
#include "Form/BaseForm.h"

#include "Utility/Debug.h"

using namespace FormUI;


__KFC_FORMS_INLINE_METHOD__
const String &Field::BaseField::getValue() const
{
    return _value;
}

__KFC_FORMS_INLINE_METHOD__
String &Field::BaseField::getValue()
{
    return _value;
}


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
const __FlashStringHelper *Field::BaseField::getName() const
{
    return FPSTR(_name);
}


__KFC_FORMS_INLINE_METHOD__
const __FlashStringHelper *Field::BaseField::getNameForType() const
{
    switch(pgm_read_byte(_name)) {
        case '.':
        case '#':
            return FPSTR(_name + 1);
    }
    return FPSTR(_name);
}


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
void Field::BaseField::setType(InputFieldType type)
{
    _type = type;
}


__KFC_FORMS_INLINE_METHOD__
InputFieldType Field::BaseField::getType() const
{
    return _type;
}

#include <debug_helper_disable.h>
