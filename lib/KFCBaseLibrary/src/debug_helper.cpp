/**
 * Author: sascha_lammers@gmx.de
 */

#if DEBUG

#include <Arduino_compat.h>
#include <PrintString.h>
#include "debug_helper.h"

String DebugHelper::__file;
String DebugHelper::__function;
uint8_t DebugHelper::__state = DEBUG_HELPER_STATE_DISABLED; // needs to be disabled until the output stream has been initialized
DebugHelperFilterVector DebugHelper::__filters;

// FixedCircularBuffer<DebugHelper::Positon_t,100> DebugHelper::__pos;

#if DEBUG_INCLUDE_SOURCE_INFO
const char ___debugPrefix[] PROGMEM = "D%08lu (%s:%u<%d> %s): ";
#else
const char ___debugPrefix[] PROGMEM = "DBG: ";
#endif

// void DebugHelper::regPrint(FixedCircularBuffer<Positon_t,100>::iterator it) {
//     while(it != __pos.end()) {
//         auto &pos = (*it);
//         DEBUG_OUTPUT.printf_P(PSTR("%s:%u - %u\n"), pos.file, pos.line, pos.time);
//         ++it;
//     }
// }

const char *DebugHelper::pretty_function(const char* name)
{
    PGM_P start = name;
    PGM_P ptr = strchr_P(name, ':');
    if (!ptr) {
        ptr = strchr_P(name, '(');
    }
    if (ptr) {
        start = ptr;
        while (start > name && pgm_read_byte(start - 1) != ' ') {
            start--;
        }
    }
    ptr = strchr_P(start, '(');
    if (ptr) {
        static String buf;
        buf = std::move(PrintString(reinterpret_cast<const __FlashBufferHelper *>(start), ptr - start));
        return buf.c_str();
    }
    return start;
}


#endif
