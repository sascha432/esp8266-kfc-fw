/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "Validator/Length.h"
#include "Field/BaseField.h"

using namespace FormUI;

bool Validator::Length::validate()
{
    if (BaseValidator::validate()) {
        size_t len = getField().getValue().length();
        auto result = (len >= _minValue && len <= _maxValue);
        if (!result && _allowEmpty) {
            String tmp = getField().getValue();
            tmp.trim();
            if (tmp.length() == 0) {
                getField().setValue(tmp);
                return true;
            }
        }
        return result;
    }
    return false;
}

String Validator::Length::getMessage()
{
    String message = BaseValidator::getMessage();
    message.replace(FSPGM(FormValidator_min_macro), String(_minValue));
    message.replace(FSPGM(FormValidator_max_macro), String(_maxValue));
    return message;
}
