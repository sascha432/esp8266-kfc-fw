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

FormRangeValidator::FormRangeValidator(long min, long max, bool allowZero) : FormRangeValidator(allowZero ? FSPGM(FormRangeValidator_default_message_zero_allowed) : FSPGM(FormRangeValidator_default_message), min, max, allowZero)
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
