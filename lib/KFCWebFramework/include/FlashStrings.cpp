/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "KFCForms.h"

#ifndef FLASH_STRING_GENERATOR_AUTO_INIT
#define FLASH_STRING_GENERATOR_AUTO_INIT(...)
#endif

FLASH_STRING_GENERATOR_AUTO_INIT(
    AUTO_STRING_DEF(FormRangeValidator_default_message, "This fields value must be between %min% and %max%")
    AUTO_STRING_DEF(FormRangeValidator_default_message_zero_allowed, "This fields value must be between %min% and %max% or 0")
    AUTO_STRING_DEF(FormLengthValidator_default_message, "This field must be between %min% and %max% characters")
    AUTO_STRING_DEF(FormEnumValidator_default_message, "Invalid value: %allowed%")
    AUTO_STRING_DEF(FormHostValidator_default_message, "Invalid hostname or IP address")
    AUTO_STRING_DEF(Form_value_missing_default_message, "This field is missing")
    AUTO_STRING_DEF(FormValidator_allowed_macro, "%allowed%")
    AUTO_STRING_DEF(FormValidator_min_macro, "%min%")
    AUTO_STRING_DEF(FormValidator_max_macro, "%max%")
);
