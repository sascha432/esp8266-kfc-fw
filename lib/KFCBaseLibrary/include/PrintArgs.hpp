/**
 * Author: sascha_lammers@gmx.de
 */

#include <PrintArgs.h>

#pragma once

inline void PrintArgs::printf_P(FormatType type)
{
    __LDBG_assert_printf((type >= FormatType::HTML_OUTPUT_BEGIN && type <= FormatType::HTML_OUTPUT_END), "type=%u invalid", type);
    _buffer.write((uint8_t)type);
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
