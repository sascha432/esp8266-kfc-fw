/**
* Author: sascha_lammers@gmx.de
*/

#include "FormField.h"
#include "FormValidator.h"
#include "Form.h"

FormField::FormField(const String &name, const String &value, InputFieldType type) : _name(name), _value(value), _type(type), _formUI(nullptr), _form(nullptr), _hasChanged(false)
{
#if DEBUG_KFC_FORMS && defined(ESP8266)
    if (name.length() >= PrintString::getSSOSIZE()) {
        debug_printf_P(PSTR("name '%s' exceeds SSOSIZE: %u >= %u. consider reducing the length to save memory\n"), name.c_str(), name.length(), PrintString::getSSOSIZE());
    }
#endif
    // _notSet = false;
    // _optional = false;
}

FormField::~FormField()
{
    for (auto validator : _validators) {
        delete validator;
    }
    _validators.clear();
    if (_formUI) {
        delete _formUI;
    }
}

void FormField::setForm(Form *form)
{
    _form = form;
}

Form &FormField::getForm() const
{
    return *_form;
}

const String &FormField::getName() const
{
    return _name;
}

const char *FormField::getNameForType() const
{
    auto ptr = _name.c_str();
    if (*ptr == '.' || *ptr == '#') {
        ptr++;
    }
    return ptr;
}

PGM_P FormField::getNameType() const
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

const String &FormField::getValue()
{
    return _value;
}

/**
* Initialize the value of the field. Should only be used in the constructor.
**/

void FormField::initValue(const String &value)
{
    _value = value;
    _hasChanged = false;
}

/**
* This method is called when the user submits a form
**/

bool FormField::setValue(const String &value)
{
    if (value != _value) {
        _value = value;
        _hasChanged = true;
    }
    return _hasChanged;
}


/*
* This method is called when the user submits a valid form. The validated data is stored
* in the value field as a string and can be retried using getValue()
**/

void FormField::copyValue()
{
    FormField::getValue();
}

bool FormField::equals(FormField *field) const
{
    return _value.equals(field->getValue());
}

bool FormField::hasChanged() const
{
    return _hasChanged;
}

void FormField::setChanged(bool hasChanged)
{
    _hasChanged = hasChanged;
}

void FormField::setType(InputFieldType type) {
    _type = type;
}

FormField::InputFieldType FormField::getType() const
{
    return _type;
}

void FormField::setFormUI(FormUI *formUI)
{
    if (_formUI) {
        delete _formUI;
    }
    _formUI = formUI;
    _formUI->setParent(this);
}

FormUI::TypeEnum_t FormField::getFormType() const
{
    if (_formUI) {
        return _formUI->getType();
    }
    return FormUI::TypeEnum_t::NONE;
}

void FormField::html(PrintInterface &output)
{
    _debug_printf_P(PSTR("name=%s formUI=%p\n"), getName().c_str(), _formUI);
    if (_formUI) {
        _formUI->html(output);
    }
}

void FormField::addValidator(FormValidator *validator)
{
    _validators.push_back(validator);
    _validators.back()->setField(this);
}

const FormField::ValidatorsVector &FormField::getValidators() const
{
    return _validators;
}

void FormGroup::end()
{
    FormUI::TypeEnum_t type;
    switch(getFormType()) {
        case FormUI::TypeEnum_t::GROUP_START:
            type = FormUI::TypeEnum_t::GROUP_END;
            break;
        case FormUI::TypeEnum_t::GROUP_START_DIV:
            type = FormUI::TypeEnum_t::GROUP_END_DIV;
            break;
        case FormUI::TypeEnum_t::GROUP_START_HR:
            type = FormUI::TypeEnum_t::GROUP_END_HR;
            break;
        default:
            return;

    }
    getForm().addGroup(getName(), String(), false, type);
}
