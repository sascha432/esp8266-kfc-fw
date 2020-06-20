/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_KFC_FORMS
#define DEBUG_KFC_FORMS         0
#endif

#include <Arduino_compat.h>

PROGMEM_STRING_DECL(FormRangeValidator_default_message);
PROGMEM_STRING_DECL(FormRangeValidator_default_message_zero_allowed);
PROGMEM_STRING_DECL(FormLengthValidator_default_message);
PROGMEM_STRING_DECL(FormEnumValidator_default_message);
PROGMEM_STRING_DECL(FormValidHostOrIpValidator_default_message);
PROGMEM_STRING_DECL(Form_value_missing_default_message);
PROGMEM_STRING_DECL(FormValidator_allowed_macro);
PROGMEM_STRING_DECL(FormValidator_min_macro);
PROGMEM_STRING_DECL(FormValidator_max_macro);
