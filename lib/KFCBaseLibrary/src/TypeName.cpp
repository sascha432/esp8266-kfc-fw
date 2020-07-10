/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "TypeName.h"

#undef ENABLE_TYPENAME

#if TYPENAME_HAVE_PROGMEM_NAME && TYPENAME_HAVE_PROGMEM_PRINT_FORMAT

#define ENABLE_TYPENAME(ID, TYPE, FORMAT, PRINT_CAST) \
    static const char TypeName_##ID##name[] PROGMEM = { #TYPE }; \
    static const char TypeName_##ID##format[] PROGMEM = { FORMAT };

ENABLE_TYPENAME_ALL();

#elif TYPENAME_HAVE_PROGMEM_NAME

#define ENABLE_TYPENAME(ID, TYPE, FORMAT, PRINT_CAST) \
    static const char TypeName_##ID##name[] PROGMEM = { #TYPE };

ENABLE_TYPENAME_ALL();

#elif TYPENAME_HAVE_PROGMEM_PRINT_FORMAT

#define ENABLE_TYPENAME(ID, TYPE, FORMAT, PRINT_CAST) \
    static const char TypeName_##ID##format[] PROGMEM = { FORMAT };

ENABLE_TYPENAME_ALL();

#endif

#if TYPENAME_HAVE_PROGMEM_NAME

const char *TypeName_name[] PROGMEM = {
    nullptr,
    TypeName_1name,
    TypeName_2name,
    TypeName_3name,
    TypeName_4name,
    TypeName_5name,
    TypeName_6name,
    TypeName_7name,
    TypeName_8name,
    TypeName_9name,
    TypeName_10name,
    TypeName_11name,
    TypeName_12name,
    TypeName_13name,
    TypeName_14name,
    TypeName_15name,
    TypeName_16name,
    TypeName_17name,
    TypeName_18name,
    TypeName_19name,
    TypeName_20name,
    TypeName_21name,
    TypeName_22name,
    TypeName_23name,
    TypeName_24name
};

const __FlashStringHelper *TypeName_getName(TypeNameEnum id)
{
    return reinterpret_cast<const __FlashStringHelper *>(TypeName_name[static_cast<int>(id)]);
}

#endif

#if TYPENAME_HAVE_PROGMEM_PRINT_FORMAT

const char *TypeName_format[] PROGMEM = {
    nullptr,
    TypeName_1format,
    TypeName_2format,
    TypeName_3format,
    TypeName_4format,
    TypeName_5format,
    TypeName_6format,
    TypeName_7format,
    TypeName_8format,
    TypeName_9format,
    TypeName_10format,
    TypeName_11format,
    TypeName_12format,
    TypeName_13format,
    TypeName_14format,
    TypeName_15format,
    TypeName_16format,
    TypeName_17format,
    TypeName_18format,
    TypeName_19format,
    TypeName_20format,
    TypeName_21format,
    TypeName_22format,
    TypeName_23format,
    TypeName_24format
};

const __FlashStringHelper *TypeName_getPrintFormat(TypeNameEnum id)
{
    return reinterpret_cast<const __FlashStringHelper *>(TypeName_format[static_cast<int>(id)]);
}

#endif
