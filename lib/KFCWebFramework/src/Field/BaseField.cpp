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

Field::BaseField::BaseField(const String &name, const String &value, Type type) :
    _name(name),
    _value(value),
    _formUI(nullptr),
    _form(nullptr),
    _type(type),
    _hasChanged(false),
    _disabled(false),
    _expanded(false)
{
#if DEBUG_KFC_FORMS && defined(ESP8266)
    if (name.length() >= PrintString::getSSOSIZE()) {
        debug_printf_P(PSTR("name '%s' exceeds SSOSIZE: %u >= %u. consider reducing the length to save memory\n"), name.c_str(), name.length(), PrintString::getSSOSIZE());
    }
#endif
}

Field::BaseField::~BaseField()
{
    if (_formUI) {
        delete _formUI;
    }
}

PGM_P Field::BaseField::getNameType() const
{
    if (_name.length() == 0) {
        return emptyString.c_str();
    }
    else if (_name.charAt(0) == '.') {
        return PSTR(" ");
    }
    return PSTR("\" id=\"");
}

/**
* Returns the value of the initialized field or changes the user submitted
**/

const String &Field::BaseField::getValue() const
{
    return _value;
}

/**
* Initialize the value of the field. Should only be used in the constructor.
**/

void Field::BaseField::initValue(const String &value)
{
    _value = value;
    _hasChanged = false;
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



WebUI::BaseUI &Field::BaseField::setFormUI(WebUI::BaseUI *formUI)
{
    if (_formUI) {
        delete _formUI;
    }
    _formUI = formUI;
    //_formUI->setParent(this);
    return *_formUI;
}

WebUI::Type Field::BaseField::getFormType() const
{
    if (_formUI) {
        return _formUI->getType();
    }
    return WebUI::Type::NONE;
}

void Field::BaseField::html(PrintInterface &output)
{
    __LDBG_printf("name=%s formUI=%p", getName().c_str(), _formUI);
    if (_formUI) {
        _formUI->html(output);
    }
}

//Validator::Vector &Field::BaseField::getValidators()
//{
//    Validator::Vector validators;
//    auto iterator = _form
//    while (_validators) {
//
//    }
//    return _getValidators();
//}

Form::BaseForm &Group::end()
{
    WebUI::Type type;
    switch(getFormType()) {
        case WebUI::Type::GROUP_START_CARD:
            type = WebUI::Type::GROUP_END_CARD;
            break;
        case WebUI::Type::GROUP_START:
            type = WebUI::Type::GROUP_END;
            break;
        case WebUI::Type::GROUP_START_DIV:
            type = WebUI::Type::GROUP_END_DIV;
            break;
        case WebUI::Type::GROUP_START_HR:
            type = WebUI::Type::GROUP_END_HR;
            break;
        default:
            return getForm();
    }
    getForm().addGroup(getName(), String(), false, type);
    return getForm();
}


#if KFC_FORMS_INCLUDE_HPP_AS_INLINE == 0

#include "Field/BaseField.hpp"

#endif
