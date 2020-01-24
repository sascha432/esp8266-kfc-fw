/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "JsonString.h"

#define DEBUG_COLLECT_STRING_ENABLE     0

#if DEBUG_COLLECT_STRING_ENABLE

#include <vector>
#include <type_traits>


extern void __debug_json_string_dump(Stream &out);
extern int __debug_json_string_skip(const String &str);
extern int __debug_json_string_add(const String &str);
extern int __debug_json_string_add(const char *str);
extern int __debug_json_string_add(const __FlashStringHelper *);

template <typename T>
int __debug_json_string_add(T str) {
    return 0;
}

extern std::vector<String> __debug_json_string_list;

#define DEBUG_COLLECT_STRING(str)               __debug_json_string_add(str)

#else

#define DEBUG_COLLECT_STRING(str)

#endif

PROGMEM_STRING_DECL(true);
PROGMEM_STRING_DECL(false);
PROGMEM_STRING_DECL(null);

class JsonTools {
public:
    static const __FlashStringHelper *boolToString(bool value);

    inline static size_t lengthEscaped(const String &value) {
        return lengthEscaped(value.c_str(), value.length());
    }
    inline static size_t lengthEscaped(const __FlashStringHelper *value) {
        return lengthEscaped(RFPSTR(value), strlen_P(RFPSTR(value)), true);
    }
    static size_t lengthEscaped(const char* value, size_t length, bool isProgMem = false);
    static size_t lengthEscaped(const JsonString &value);

    inline static size_t printToEscaped(Print &output, const String &value) {
        return printToEscaped(output, value.c_str(), value.length());
    }
    inline static size_t printToEscaped(Print &output, const __FlashStringHelper *value) {
        return printToEscaped(output, RFPSTR(value), strlen_P(RFPSTR(value)), true);
    }
    static size_t printToEscaped(Print &output, const char* value, size_t length, bool isProgMem = false);
    static size_t printToEscaped(Print &output, const JsonString &value);

    inline static uint8_t printEscaped(Print &output, char ch) {
        if (!output.write('\\')) {
            return 0;
        }
        uint8_t len = 0;
        switch (ch) {
        case '\b':
            len = (uint8_t)output.write('b');
            break;
        case '\f':
            len = (uint8_t)output.write('f');
            break;
        case '\t':
            len = (uint8_t)output.write('t');
            break;
        case '\r':
            len = (uint8_t)output.write('r');
            break;
        case '\n':
            len = (uint8_t)output.write('n');
            break;
        case '\\':
            len = (uint8_t)output.write('\\');
            break;
        case '"':
            len = (uint8_t)output.write('"');
            break;
        }
        if (!len) {
            return 1;
        }
        return 2;
    }
};

