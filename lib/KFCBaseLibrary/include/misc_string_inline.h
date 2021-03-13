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


// #if defined(_MSC_VER)
// inline void *memrchr(const void *s, int c, size_t n)
// {
//     const unsigned char *cp;
//     if (!n) {
//         return NULL;
//     }
//     cp = (unsigned char *)s + n;
//     do {
//         if (*(--cp) == (unsigned char)c) {
//             return (void *)cp;
//         }
//     } while (--n != 0);
//     return NULL;
// }
// inline void *memrchr_P(const void *s, int c, size_t n) {
//     return memrchr(s, c, n);
// }
// #endif

inline char *strrstr(char *string, const char *find)
{
    if (!string || !find) {
        return nullptr;
    }
    return __strrstr(string, strlen(string), find, strlen(find));
}

inline char *strrstr_P(char *string, const char *find)
{
    if (!string || !find) {
        return nullptr;
    }
    return __strrstr_P(string, strlen(string), find, strlen_P(find));
}

inline const char *strrstr_P_P(const char *string, const char *find)
{
    if (!string || !find) {
        return nullptr;
    }
    return __strrstr_P_P(string, strlen_P(string), find, strlen_P(find));
}

inline char *__strrstr(char *string, size_t stringLen, const char *find, size_t findLen)
{
	if (findLen > stringLen)
		return nullptr;

	for (auto ptr = string + stringLen - findLen; ptr >= string; ptr--) {
		if (strncmp(ptr, find, findLen) == 0) {
			return ptr;
        }
    }
	return nullptr;
}

inline char *__strrstr_P(char *string, size_t stringLen, const char *find, size_t findLen)
{
	if (findLen > stringLen) {
        return nullptr;
    }

	for (auto ptr = string + stringLen - findLen; ptr >= string; ptr--) {
		if (strncmp_P(ptr, find, findLen) == 0) {
			return ptr;
        }
    }
	return nullptr;
}

inline const char *__strrstr_P_P(const char *string, size_t stringLen, const char *find, size_t findLen)
{
	if (findLen > stringLen) {
        return nullptr;
    }

	for (auto ptr = string + stringLen - findLen; ptr >= string; ptr--) {
		if (strncmp_P_P(ptr, find, findLen) == 0) {
			return ptr;
        }
    }
	return nullptr;
}

//
//inline int String_indexOf(const String &str, const __FlashStringHelper *find, size_t fromIndex)
//{
//    if (!find) {
//        return -1;
//    }
//    auto ptr = str.c_str();
//    if (fromIndex >= str.length()) {
//        return -1;
//    }
//    auto idxPtr = strstr_P(ptr + fromIndex, reinterpret_cast<PGM_P>(find));
//    if (!idxPtr) {
//        return -1;
//
//    }
//    return idxPtr - ptr;
//}

/*inline int String_lastIndexOf(const String &str, char find)
{
    auto ptr = strrchr(str.c_str(), find);
    if (!ptr) {
        return -1;
    }
    return ptr - str.c_str();
}

inline int String_lastIndexOf(const String &str, char find, size_t fromIndex)
{
    if (!find) {
        return -1;
    }
    if (fromIndex == ~0U) {
        fromIndex = str.length();
    }
    else if (fromIndex > str.length() || fromIndex < 1) {
        return -1;
    }
    auto ptr = reinterpret_cast<const char *>(memrchr(str.c_str(), find, fromIndex));
    if (!ptr) {
        return -1;
    }
    return ptr - str.c_str();
}

inline int String_lastIndexOf(const String &str, const __FlashStringHelper *find)
{
    if (!find || !str.length()) {
        return -1;
    }
    auto findLen = strlen_P(reinterpret_cast<PGM_P>(find));
    if (!findLen) {
        return -1;
    }
    auto ptr = __strrstr_P(const_cast<char *>(str.c_str()), str.length() - findLen, reinterpret_cast<PGM_P>(find), findLen);
    if (!ptr) {
        return -1;
    }
    return ptr - str.c_str();
}

inline int String_lastIndexOf(const String &str, const __FlashStringHelper *find, size_t fromIndex)
{
    if (!find) {
        return -1;
    }
    return String_lastIndexOf(str, find, fromIndex, strlen_P(reinterpret_cast<PGM_P>(find)));
}

inline int String_lastIndexOf(const String &str, const __FlashStringHelper *find, size_t fromIndex, size_t findLen)
{
    size_t len;
    if (!find || !(len = str.length())) {
        return -1;
    }
    if (fromIndex == ~0U) {
        fromIndex = len;
    }
    else if (fromIndex < findLen || fromIndex > len) {
        return -1;
    }
    auto ptr = __strrstr_P(const_cast<char *>(str.c_str()), fromIndex + findLen, reinterpret_cast<PGM_P>(find), findLen);
    if (!ptr) {
        return -1;
    }
    return ptr - str.c_str();
}

inline int String_lastIndexOf(const String &str, const char *find, size_t fromIndex, size_t findLen)
{
    size_t len;
    if (!find || !(len = str.length())) {
        return -1;
    }
    if (fromIndex == ~0U) {
        fromIndex = len;
    }
    else if (fromIndex < findLen || fromIndex > len) {
        return -1;
    }
    auto ptr = __strrstr(const_cast<char *>(str.c_str()), fromIndex + findLen, find, findLen);
    if (!ptr) {
        return -1;
    }
    return ptr - str.c_str();
}

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
}
*/

//inline bool String_equals_P(const String &str1, PGM_P str2)
//{
//    return !strcmp_P(str1.c_str(), str2);
//}
//
//inline bool String_equalsIgnoreCase_P(const String &str1, PGM_P str2)
//{
//    return !strcasecmp_P(str1.c_str(), str2);
//}
//
//
//inline bool String_equalsIgnoreCase(const String &str1, const __FlashStringHelper *str2)
//{
//    return String_equalsIgnoreCase(str1, reinterpret_cast<PGM_P>(str2));
//}
////
//
//inline bool String_startsWith(const String &str1, const __FlashStringHelper *str2)
//{
//    return String_startsWith(str1, reinterpret_cast<PGM_P>(str2));
//}
//
//inline bool String_endsWith(const String &str1, const __FlashStringHelper *str2)
//{
//    return String_endsWith(str1, reinterpret_cast<PGM_P>(str2));
//}
