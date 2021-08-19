/**
 * Author: sascha_lammers@gmx.de
 */

#include <pgmspace.h>
#include "PrintArgs.h"

#pragma once

inline void PrintArgs::vprintf_P(const uintptr_t format, const uintptr_t *args, size_t numArgs)
{
    __LDBG_assert_printf(numArgs <= kMaximumPrintfArguments, "num_args=%u exceeds max=%u", numArgs, kMaximumPrintfArguments);
    _buffer.write(static_cast<uint8_t>(numArgs));
    _buffer.push_back(format);
    _buffer.write(reinterpret_cast<const uint8_t *>(args), sizeof(uintptr_t) * numArgs);
}

inline void PrintArgs::print(const char *str)
{
    print(FPSTR(str));
}

inline void PrintArgs::print(const __FlashStringHelper *fpstr)
{
    _buffer.write(static_cast<uint8_t>(FormatType::SINGLE_STRING));
    _buffer.push_back(reinterpret_cast<uintptr_t>(const_cast<__FlashStringHelper *>(fpstr)));
}

inline void PrintArgs::println(const char *str)
{
    print(FPSTR(str));
    _buffer.write(static_cast<uint8_t>(FormatType::SINGLE_CRLF));
}

inline void PrintArgs::println(const __FlashStringHelper *fpstr)
{
    print(fpstr);
    _buffer.write(static_cast<uint8_t>(FormatType::SINGLE_CRLF));
}

inline void PrintArgs::_initOutput()
{
    if (!_bufferPtr) {
        _bufferPtr = _buffer.begin();
        _position = 0;
    }
}

inline int PrintArgs::_snprintf_P(uint8_t *buffer, size_t size, const uintptr_t *args, uint8_t numArgs)
{
    #if DEBUG_PRINT_ARGS && 0
        String fmt = F("format='%s' args[");
        fmt += String((numArgs - 1));
        fmt += "]={";
        for(uint8_t i = 1; i < numArgs; i++) {
            fmt += F("%08x, ");
        }
        String_rtrim_P(fmt, F(", "));
        fmt += F("}\n");
        debug_printf(fmt.c_str(), (PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
    #endif
    #if DEBUG_PRINT_ARGS
        _stats.calls.numArgs[numArgs]++;
    #endif
    if (numArgs <= 4) { // most frequently used are 3 and 4
        return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], (PGM_P)args[1], (PGM_P)args[2], (PGM_P)args[3]);
    }
    if (numArgs <= 8) {
        return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], (PGM_P)args[1], (PGM_P)args[2], (PGM_P)args[3], (PGM_P)args[4], (PGM_P)args[5], (PGM_P)args[6], (PGM_P)args[7]);
    }
    return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], (PGM_P)args[1], (PGM_P)args[2], (PGM_P)args[3], (PGM_P)args[4], (PGM_P)args[5], (PGM_P)args[6], (PGM_P)args[7], (PGM_P)args[8], (PGM_P)args[9], (PGM_P)args[10], (PGM_P)args[11], (PGM_P)args[12], (PGM_P)args[13], (PGM_P)args[14], (PGM_P)args[15]);
}
