/**
* Author: sascha_lammers@gmx.de
*/

#include "FormError.h"
#include "FormField.h"

FormError::FormError() {
    _field = nullptr;
}

FormError::FormError(const String & message) {
    _field = nullptr;
    _message = message;
}

FormError::FormError(FormField * field, const String & message) {
    _field = field;
    _message = message;
}

const String & FormError::getName() {
    if (!_field) {
        return _sharedEmptyString;
    }
    return _field->getName();
}

const String & FormError::getMessage() {
    return _message;
}

const bool FormError::is(FormField * field) const {
    return field == _field;
}
