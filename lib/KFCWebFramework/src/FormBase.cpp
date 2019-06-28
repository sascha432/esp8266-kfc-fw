/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "FormBase.h"

PROGMEM_STRING_DEF(FormRangeValidator_default_message, "This fields value must be between %min% and %max%");
PROGMEM_STRING_DEF(FormLengthValidator_default_message, "This field must be between %min% and %max% characters");
PROGMEM_STRING_DEF(FormEnumValidator_default_message, "Invalid value: %allowed%");
PROGMEM_STRING_DEF(FormValidHostOrIpValidator_default_message, "Invalid hostname or IP address");
PROGMEM_STRING_DEF(Form_value_missing_default_message, "This field is missing");
PROGMEM_STRING_DEF(FormValidator_allowed_macro, "%allowed%");
PROGMEM_STRING_DEF(FormValidator_min_macro, "%min%");
PROGMEM_STRING_DEF(FormValidator_max_macro, "%max%");
