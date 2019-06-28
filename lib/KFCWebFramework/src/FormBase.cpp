/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include "FormBase.h"

#if _WIN32 || _WIN64

const char *FormRangeValidator_default_message = "This fields value must be between %min% and %max%";
const char *FormLengthValidator_default_message = "This field must be between %min% and %max% characters";
const char *FormEnumValidator_default_message = "Invalid value: %allowed%";
const char *FormValidHostOrIpValidator_default_message = "Invalid hostname or IP address";
const char *Form_value_missing_default_message = "This field is missing";
const char *FormValidator_allowed_macro = "%allowed%";
const char *FormValidator_min_macro = "%min%";
const char *FormValidator_max_macro = "%max%";

#else

const char FormRangeValidator_default_message[] PROGMEM = { "This fields value must be between %min% and %max%" };
const char FormLengthValidator_default_message[] PROGMEM = { "This field must be between %min% and %max% characters" };
const char FormEnumValidator_default_message[] PROGMEM = { "Invalid value: %allowed%" };
const char FormValidHostOrIpValidator_default_message[] PROGMEM = { "Invalid hostname or IP address" };
const char Form_value_missing_default_message[] PROGMEM = { "This field is missing" };
const char FormValidator_allowed_macro[] PROGMEM = { "%allowed%" };
const char FormValidator_min_macro[] PROGMEM = { "%min%" };
const char FormValidator_max_macro[] PROGMEM = { "%max%" };

#endif