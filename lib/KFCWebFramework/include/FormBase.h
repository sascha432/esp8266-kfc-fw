/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

PROGMEM_STRING_DECL(FormRangeValidator_default_message);
PROGMEM_STRING_DECL(FormLengthValidator_default_message);
PROGMEM_STRING_DECL(FormEnumValidator_default_message);
PROGMEM_STRING_DECL(FormValidHostOrIpValidator_default_message);
PROGMEM_STRING_DECL(Form_value_missing_default_message);
PROGMEM_STRING_DECL(FormValidator_allowed_macro);
PROGMEM_STRING_DECL(FormValidator_min_macro);
PROGMEM_STRING_DECL(FormValidator_max_macro);

#define DEBUG_FORMS 0

#if DEBUG_FORMS
#define DEBUG_FORMS_PREFIX "Forms: "
#define if_debug_forms_printf_P           debug_printf_P
#else
#define DEBUG_FORMS_PREFIX "Forms: "
#define if_debug_forms_printf_P(...) ;
#endif
