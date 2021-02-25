#pragma once

#include <stdint.h>
#include <strings.h>
#include <pgmspace.h>

class String;
class __FlashStringHelper;
#ifndef PGM_P
#define PGM_P const char *
#endif

inline size_t String_rtrim(String &str, const __FlashStringHelper *chars, size_t minLength)
{
    return String_rtrim_P(str, reinterpret_cast<PGM_P>(chars), minLength);
}

inline size_t String_rtrim(String &str, const __FlashStringHelper *chars)
{
    return String_rtrim_P(str, reinterpret_cast<PGM_P>(chars), ~0U);
}

inline size_t String_ltrim(String &str, const __FlashStringHelper *chars)
{
    return String_ltrim_P(str, reinterpret_cast<PGM_P>(chars));
}

inline size_t String_trim(String &str, const __FlashStringHelper *chars)
{
    return String_trim_P(str, reinterpret_cast<PGM_P>(chars));
}


inline bool String_equals(const String &str1, PGM_P str2)
{
    return !strcmp_P(str1.c_str(), str2);
}

inline bool String_equalsIgnoreCase(const String &str1, PGM_P str2)
{
    return !strcasecmp_P(str1.c_str(), str2);
}


inline bool String_equals_P(const String &str1, PGM_P str2)
{
    return !strcmp_P(str1.c_str(), str2);
}

inline bool String_equalsIgnoreCase_P(const String &str1, PGM_P str2)
{
    return !strcasecmp_P(str1.c_str(), str2);
}


inline bool String_equals(const String &str1, const __FlashStringHelper *str2)
{
    return String_equals(str1, reinterpret_cast<PGM_P>(str2));
}

inline bool String_equalsIgnoreCase(const String &str1, const __FlashStringHelper *str2)
{
    return String_equalsIgnoreCase(str1, reinterpret_cast<PGM_P>(str2));
}


inline bool String_startsWith(const String &str1, const __FlashStringHelper *str2)
{
    return String_startsWith(str1, reinterpret_cast<PGM_P>(str2));
}

inline bool String_endsWith(const String &str1, const __FlashStringHelper *str2)
{
    return String_endsWith(str1, reinterpret_cast<PGM_P>(str2));
}
