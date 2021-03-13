/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdint.h>
#include <string.h>
#include <pgmspace.h>

class String;
class __FlashStringHelper;
#ifndef PGM_P
#define PGM_P const char *
#endif

#if defined(_MSC_VER)

namespace esp8266 {
    class String;
}

inline void *memrchr(const void *s, int c, size_t n)
{
    const unsigned char *cp;
    if (!n) {
        return nullptr;
    }
    cp = (unsigned char *)s + n;
    do {
        if (*(--cp) == (unsigned char)c) {
            return (void *)cp;
        }
    } while (--n != 0);
    return nullptr;
}

#endif
inline void *memrchr_P(const void *s, int c, size_t n) {
    return memrchr(s, c, n);
}

int strcmp_P_P(const char *str1, const char *str2);
int strncmp_P_P(const char *str1, const char *str2, size_t len);
int strcasecmp_P_P(const char *str1, const char *str2);
int strncasecmp_P_P(const char *str1, const char *str2, size_t size);

const char *strstr_P_P(const char *str, const char *find);
const char *strcasestr_P_P(const char *str, const char *find);

const char *strchr_P(const char *str, int c);
const char *strrchr_P(const char *str, int c);

inline char *strrstr(char *string, const char *find);
inline const char *strrstr(const char *string, const char *find) {
    return strrstr(const_cast<char *>(string), find);
}
inline char *strrstr_P(char *string, const char *find);
inline const char *strrstr_P(const char *string, const char *find) {
    return strrstr_P(const_cast<char *>(string), find);
}
inline const char *strrstr_P_P(const char *string, const char *find);
inline char *__strrstr(char *string, size_t stringLen, const char *find, size_t findLen);
inline const char *__strrstr(const char *string, size_t stringLen, const char *find, size_t findLen) {
    return __strrstr(string, stringLen, find, findLen);
}
inline char *__strrstr_P(char *string, size_t stringLen, const char *find, size_t findLen);
inline const char *__strrstr_P(const char *string, size_t stringLen, const char *find, size_t findLen) {
    return __strrstr_P(string, stringLen, find, findLen);
}
inline const char *__strrstr_P_P(const char *string, size_t stringLen, const char *find, size_t findLen);


// inline int String_indexOf(const String &str, const __FlashStringHelper *find, size_t fromIndex = 0);

// inline int String_lastIndexOf(const String &str, char find);
// inline int String_lastIndexOf(const String &str, char find, size_t fromIndex);
// inline int String_lastIndexOf(const String &str, const char *find);
// inline int String_lastIndexOf(const String &str, const char *find, size_t fromIndex);
// inline int String_lastIndexOf(const String &str, const char *find, size_t fromIndex, size_t findLen);
// inline int String_lastIndexOf(const String &str, const __FlashStringHelper *find);
// inline int String_lastIndexOf(const String &str, const __FlashStringHelper *find, size_t fromIndex);
// inline int String_lastIndexOf(const String &str, const __FlashStringHelper *find, size_t fromIndex, size_t findLen);

// inline size_t String_replace(String &str, int from, int to);
// inline size_t String_replaceIgnoreCase(String &str, int from, int to);

// size_t String_ltrim(String &str);
// size_t String_ltrim(String &str, const char *chars);
// size_t String_ltrim_P(String &str, PGM_P chars);
// size_t String_ltrim(String &str, char chars);
// inline size_t String_ltrim(String &str, const __FlashStringHelper *chars);

// size_t String_rtrim(String &str);
// size_t String_rtrim(String &str, const char *chars, size_t minLength = ~0);
// size_t String_rtrim_P(String &str, PGM_P chars, size_t minLength = ~0);
// size_t String_rtrim(String &str, char chars, size_t minLength = ~0);
// inline size_t String_rtrim(String &str, const __FlashStringHelper *chars);
// inline size_t String_rtrim(String &str, const __FlashStringHelper *chars, size_t minLength);

// size_t String_trim(String &str);
// size_t String_trim(String &str, const char *chars);
// size_t String_trim_P(String &str, PGM_P chars);
// size_t String_trim(String &str, char chars);
// inline size_t String_trim(String &str, const __FlashStringHelper *chars);

// bool String_equals(const String &str1, PGM_P str2);
// bool String_equalsIgnoreCase(const String &str1, PGM_P str2);
// //bool String_equals(const String &str1, const __FlashStringHelper *str2);
// bool String_equalsIgnoreCase(const String &str1, const __FlashStringHelper *str2);

// bool String_startsWith(const String &str1, const __FlashStringHelper *str2);
// bool String_startsWith(const String &str1, PGM_P str2);
// bool String_endsWith(const String &str1, const __FlashStringHelper *str2);
// bool String_endsWith(const String &str1, PGM_P str2);
