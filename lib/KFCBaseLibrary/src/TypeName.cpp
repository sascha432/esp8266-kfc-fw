/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "TypeName.h"

#undef ENABLE_TYPENAME

#define ENABLE_TYPENAME(T, SNAME, FORMAT, NAME) \
    const char TypeName_details_##NAME##name[] PROGMEM = { #T }; \
    const char TypeName_details_##NAME##sname[] PROGMEM = { #SNAME }; \
    const char TypeName_details_##NAME##format[] PROGMEM = { FORMAT }; \
    const __FlashStringHelper * TypeName<T>::name() { return FPSTR(TypeName_details_##NAME##name); } \
    const __FlashStringHelper* TypeName<T>::sname() { return FPSTR(TypeName_details_##NAME##sname); } \
    const __FlashStringHelper* TypeName<T>::format() { return FPSTR(TypeName_details_##NAME##format); } \
    const char *TypeName<T>::name_cstr() { return #T; }

ENABLE_TYPENAME_ALL();
