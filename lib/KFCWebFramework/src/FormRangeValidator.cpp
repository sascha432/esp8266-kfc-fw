/**
* Author: sascha_lammers@gmx.de
*/

#include "FormRangeValidator.h"
#include "FormField.h"
#include "Form.h"

#ifdef _max
#undef _max
#undef _min
#endif

static const __FlashStringHelper *getDefaultMessage(bool allowZero) {
    return allowZero ? FSPGM(FormRangeValidator_default_message_zero_allowed) : FSPGM(FormRangeValidator_default_message);
}

FormRangeValidator::FormRangeValidator(long min, long max, bool allowZero) : FormRangeValidator(getDefaultMessage(allowZero), min, max, allowZero)
{
}

FormRangeValidator::FormRangeValidator(const String &message, long min, long max, bool allowZero) : FormValidator(message.length() == 0 ? String(getDefaultMessage(allowZero)) : message), _min(min), _max(max), _allowZero(allowZero)
{
}

bool FormRangeValidator::validate()
{
    if (FormValidator::validate()) {
        long value = getField().getValue().toInt();
        __LDBG_printf("name=%s", getField().getName().c_str());
        __LDBG_printf("value=%ld min=%ld max=%ld allo_zero=%u", value, _min, _max, _allowZero);
        if (FormValidator::validate()) {
            return (_allowZero && value == 0) || (value >= _min && value <= _max);
        }
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

FormNetworkPortValidator::FormNetworkPortValidator(long min, long max, bool allowZero) : FormRangeValidator((min == kPortMin && max == kPortMax) ? FSPGM(FormRangeValidator_invalid_port, "Invalid Port") : FSPGM(FormRangeValidator_invalid_port_range, "Invalid Port (%min%-%max%)"), min, max, allowZero)
{
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
