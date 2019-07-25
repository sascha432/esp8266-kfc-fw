/**
* Author: sascha_lammers@gmx.de
*/

#include "FormField.h"
#include "FormValidator.h"

FormField::FormField(const String & name) {
    _name = name;
    _hasChanged = false;
    _type = FormField::INPUT_NONE;
    // _notSet = false;
    // _optional = false;
}

FormField::FormField(const String & name, const String & value) : FormField(name) {
    initValue(value);
}

FormField::FormField(const String & name, const String & value, FieldType_t type) : FormField(name, value) {
    _type = type;
}

// FormField(const String &name, const String &value, bool optional) : FormField(name, value) {
//     _optional = optional;
// }

FormField::~FormField() {
    for (auto validator : _validators) {
        delete validator;
    }
    _validators.clear();
}

void FormField::setForm(Form * form) {
    _form = form;
}

Form &FormField::getForm() const {
    return *_form;
}

const String & FormField::getName() const {
    return _name;
}

/**
* Returns the value of the initialized field or changes the user submitted
**/

const String & FormField::getValue() {
    return _value;
}

/**
* Initialize the value of the field. Should only be used in the constructor.
**/

void FormField::initValue(const String & value) {
    _value = value;
    _hasChanged = false;
}

/**
* This method is called when the user submits a form
**/

bool FormField::setValue(const String & value) {
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

void FormField::copyValue() {
    String storedValue = FormField::getValue();
}

const bool FormField::equals(FormField * field) const {
    return _value.equals(field->getValue());
}

const bool FormField::hasChanged() const {
    return _hasChanged;
}

void FormField::setChanged(bool hasChanged) {
    _hasChanged = hasChanged;
}

void FormField::setType(FieldType_t type) {
    _type = type;
}

const FormField::FieldType_t FormField::getType() const {
    return _type;
}

void FormField::addValidator(FormValidator * validator) {
    _validators.push_back(validator);
    _validators.back()->setField(this);
}

const std::vector<FormValidator*>& FormField::getValidators() const {
    return _validators;
}
