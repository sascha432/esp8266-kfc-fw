/**
 * Author: sascha_lammers@gmx.de
 */

#if DEBUG

#include "debug_helper.h"

String DebugHelper::__file;
unsigned DebugHelper::__line = 0;
String DebugHelper::__function;
uint8_t DebugHelper::__state = DEBUG_HELPER_STATE_DISABLED; // needs to be disabled until the output stream has been initialized
DebugHelperFilterVector DebugHelper::__filters;

#if DEBUG_INCLUDE_SOURCE_INFO
const char ___debugPrefix[] PROGMEM = "DEBUG%08lu (%s:%u<%d> %s): ";
#else
const char ___debugPrefix[] PROGMEM = "DEBUG: ";
#endif
String _debugPrefix = ___debugPrefix;

const char *DebugHelper::basename(const String &file) {
    const char *p = strrchr(file.c_str(), '/');
    if (!p) {
        p = strrchr(file.c_str(), '\\');
    }
    return p ? p + 1   : file.c_str();
}


#endif
