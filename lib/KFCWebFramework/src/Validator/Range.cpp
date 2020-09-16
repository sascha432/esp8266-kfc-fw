/**
* Author: sascha_lammers@gmx.de
*/

#include "Validator/Range.h"
#include "Field/BaseField.h"
#include "Form/BaseForm.h"
#include "Form/Form.hpp"

#include "Utility/Debug.h"

String FormUI::Validator::RangeMessage::getDefaultMessage(const String &message, bool allowZero) {
    if (message.length()) {
        return message;
    }
    return allowZero ? FSPGM(FormRangeValidator_default_message_zero_allowed) : FSPGM(FormRangeValidator_default_message);
}
