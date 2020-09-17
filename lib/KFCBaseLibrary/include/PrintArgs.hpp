/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintArgs.h>

#pragma once

inline void PrintArgs::printf_P(FormatType type)
{
    __LDBG_assert_printf((type >= FormatType::HTML_OUTPUT_BEGIN && type <= FormatType::HTML_OUTPUT_END), "type=%u invalid", type);
    _buffer.write(static_cast<uint8_t>(type));
}


inline void PrintArgs::print(char data)
{
    _buffer.write(static_cast<uint8_t>(FormatType::SINGLE_CHAR));
    _buffer.write(static_cast<uint8_t>(data));
}

inline void PrintArgs::print(const char *str)
{
    print(FPSTR(str));
}

inline void PrintArgs::print(const __FlashStringHelper *fpstr)
{
    _buffer.write(static_cast<uint8_t>(FormatType::SINGLE_STRING));
    _buffer.push_back((uintptr_t)fpstr);
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

inline int PrintArgs::_snprintf_P(uint8_t *buffer, size_t size, uintptr_t **args, uint8_t numArgs)
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
    if (numArgs < 4) {
        return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], args[1], args[2], args[3]);
    }
    if (numArgs < 8) {
        return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    }
    return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
}
