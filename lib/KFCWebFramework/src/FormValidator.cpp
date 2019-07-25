/**
* Author: sascha_lammers@gmx.de
*/

#include "FormValidator.h"
#include "FormField.h"
#include "Form.h"

FormValidator::FormValidator() {
    _validateIfValid = false;
}

FormValidator::FormValidator(const String & message) : FormValidator() {
    _message = message;
}

FormValidator::~FormValidator() {
}

void FormValidator::setField(FormField * field) {
    _field = field;
}

FormField & FormValidator::getField() {
    return *_field;
}

String FormValidator::getMessage() {
    return _message;
}

bool FormValidator::validate() {
    if (_validateIfValid) {
        if (!_field->getForm().isValid()) {
            return false;
        }
    }
    return true;
}

void FormValidator::setValidateIfValid(bool validateIfValid) {
    _validateIfValid = validateIfValid;
}
