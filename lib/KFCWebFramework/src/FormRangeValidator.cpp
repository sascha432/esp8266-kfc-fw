/**
* Author: sascha_lammers@gmx.de
*/

#include "FormRangeValidator.h"
#include "FormField.h"
#include "FormBase.h"

FormRangeValidator::FormRangeValidator(long min, long max) : FormRangeValidator(FPSTR(FormRangeValidator_default_message), min, max) {
}

FormRangeValidator::FormRangeValidator(const String & message, long min, long max) : FormValidator(message) {
    _min = min;
    _max = max;
}

bool FormRangeValidator::validate() {
    if (FormValidator::validate()) {
        long value = getField()->getValue().toInt();
        return (value >= _min && value <= _max);
    }
    return false;
}

String FormRangeValidator::getMessage() {
    String message = FormValidator::getMessage();
    message.replace(FPSTR(FormValidator_min_macro), String(_min));
    message.replace(FPSTR(FormValidator_max_macro), String(_max));
    return message;
}
