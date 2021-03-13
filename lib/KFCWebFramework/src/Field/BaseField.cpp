/**
* Author: sascha_lammers@gmx.de
*/

#include "Field/BaseField.h"
#include "Validator/BaseValidator.h"
#include "Form/BaseForm.h"
#include "WebUI/BaseUI.h"
#include "WebUI/Containers.h"
#include "Form/Form.hpp"

using namespace FormUI;

Field::BaseField::~BaseField()
{
    if (_formUI) {
        delete _formUI;
    }
}

const __FlashStringHelper *Field::BaseField::getNameType() const
{
    auto ch = pgm_read_byte(_name);
    if (!ch) {
        return FPSTR(emptyString.c_str());
    }
    else if (ch == '.') {
        return F(" ");
    }
    return F("\" id=\"");
}


/**
* This method is called when the user submits a form
**/
bool Field::BaseField::setValue(const String &value)
{
    if (value != _value) {
        _value = value;
        _hasChanged = true;
    }
    return _hasChanged;
}

// class Group : Field::BaseField
//
// close this group
Form::BaseForm &Group::end()
{
    auto &form = getForm();
    if (_groupOpen == false) {
        __LDBG_assert_printf(F("group already closed") == nullptr, "group=%s already closed", getName());
        return form;
    }

    // get RenderType for closing the group
    RenderType type = getRenderType();
    __LDBG_assert_printf(form.getGroupType(type) == Form::BaseForm::GroupType::OPEN, "invalid type=%u group=%u name=%s", type, form.getGroupType(type), getName());

    _groupOpen = false;
    form.endGroup(FPSTR(getName()), form.getEndGroupType(type));
    return form;
}


#if KFC_FORMS_INCLUDE_HPP_AS_INLINE == 0

#include "Field/BaseField.hpp"

#endif
