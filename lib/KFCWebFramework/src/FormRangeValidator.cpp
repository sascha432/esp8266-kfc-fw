/**
* Author: sascha_lammers@gmx.de
*/

#include "FormRangeValidator.h"
#include "FormField.h"
#include "FormBase.h"

#ifdef _max
#undef _max
#undef _min
#endif

FormRangeValidator::FormRangeValidator(long min, long max, bool allowZero) :
    FormRangeValidator(allowZero ? FSPGM(FormRangeValidator_default_message_zero_allowed) : FSPGM(FormRangeValidator_default_message), min, max, allowZero)
{
}

FormRangeValidator::FormRangeValidator(const String & message, long min, long max, bool allowZero) : FormValidator(message), _min(min), _max(max), _allowZero(allowZero)
{
}

bool FormRangeValidator::validate()
{
    if (FormValidator::validate()) {
        long value = getField().getValue().toInt();
        return (_allowZero && value == 0) || (value >= _min && value <= _max);
    }
    return false;
}

String FormRangeValidator::getMessage()
{
    String message = FormValidator::getMessage();
    message.replace(FSPGM(FormValidator_min_macro), String(_min));
    message.replace(FSPGM(FormValidator_max_macro), String(_max));
    return message;
}


FormRangeValidatorDouble::FormRangeValidatorDouble(double min, double max, uint8_t digits, bool allowZero) : FormRangeValidatorDouble(allowZero ? FSPGM(FormRangeValidator_default_message_zero_allowed) : FSPGM(FormRangeValidator_default_message), min, max, digits, allowZero)
{
}

FormRangeValidatorDouble::FormRangeValidatorDouble(const String &message, double min, double max, uint8_t digits, bool allowZero) : FormValidator(message), _min(min), _max(max), _digits(digits), _allowZero(allowZero)
{
}

bool FormRangeValidatorDouble::validate()
{
    if (FormValidator::validate()) {
        long value = getField().getValue().toInt();
        return (_allowZero && value == 0) || (value >= _min && value <= _max);
    }
    return false;
}

String FormRangeValidatorDouble::getMessage()
{
    String message = FormValidator::getMessage();
    message.replace(FSPGM(FormValidator_min_macro), String(_min, _digits));
    message.replace(FSPGM(FormValidator_max_macro), String(_max, _digits));
    return message;
}
