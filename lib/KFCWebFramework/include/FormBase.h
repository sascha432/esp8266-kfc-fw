/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#if _WIN32 || _WIN64

extern const char *FormRangeValidator_default_message;
extern const char *FormLengthValidator_default_message;
extern const char *FormEnumValidator_default_message;
extern const char *FormValidHostOrIpValidator_default_message;
extern const char *Form_value_missing_default_message;
extern const char *FormValidator_allowed_macro;
extern const char *FormValidator_min_macro;
extern const char *FormValidator_max_macro;

#else
extern const char FormRangeValidator_default_message[] PROGMEM;
extern const char FormLengthValidator_default_message[] PROGMEM;
extern const char FormEnumValidator_default_message[] PROGMEM;
extern const char FormValidHostOrIpValidator_default_message[] PROGMEM;
extern const char Form_value_missing_default_message[] PROGMEM;
extern const char FormValidator_allowed_macro[] PROGMEM;
extern const char FormValidator_min_macro[] PROGMEM;
extern const char FormValidator_max_macro[] PROGMEM;

#endif

#define DEBUG_FORMS 0

#if DEBUG_FORMS
#define DEBUG_FORMS_PREFIX "Forms: "
#define if_debug_forms_printf_P           debug_printf_P
#else
#define if_debug_forms_printf_P(...) ;
#endif

