/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdint.h>
#include <string.h>
#include <pgmspace.h>
#include <memory>

class String;
class __FlashStringHelper;
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef PGM_VOID_P
#define PGM_VOID_P const void *
#endif

#define STRINGLIST_SEPARATOR                    ','
#define STRLS                                   ","
#define STRINGLIST_ITEM_NOT_FOUND(result)       (result < 0)
#define STRINGLIST_ITEM_FOUND(result)           (result >= 0)
#define STRL_OK(result)                         STRINGLIST_ITEM_FOUND(result)
#define STRL_FAIL(result)                       STRINGLIST_ITEM_NOT_FOUND(result)

#define ESNULLP                                 ( 400 ) /* null ptr */
#define ESZEROL                                 ( 401 ) /* length is zero */
#define EOK                                     ( 0 )

// return index of the string in the list, 0 based
// all negative values are errors
// list == nullptr or find == nullptr: returns -1
// separator == 0 or nullptr: returns -1
int stringlist_find_P_P(PGM_P list, PGM_P find, PGM_P separator);
int stringlist_ifind_P_P(PGM_P list, PGM_P find, PGM_P separator);

inline static int stringlist_find_P(const __FlashStringHelper *list, const char *find, const __FlashStringHelper *separator)
{
    return stringlist_find_P_P(reinterpret_cast<PGM_P>(list), find, reinterpret_cast<PGM_P>(separator));
}

inline static int stringlist_ifind_P(const __FlashStringHelper *list, const char *find, const __FlashStringHelper *separator)
{
    return stringlist_ifind_P_P(reinterpret_cast<PGM_P>(list), find, reinterpret_cast<PGM_P>(separator));
}

inline static int stringlist_find_P_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR)
{
    if (!separator) {
        return -1;
    }
    if (!list || !find) {
        return -1;
    }
    const char separator_str[2] = { separator, 0 };
    return stringlist_find_P_P(list, find, separator_str);
}

inline static int stringlist_find_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_find_P_P(list, find, separator);
}

inline static int stringlist_find_P(const __FlashStringHelper *list, const char *find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_find_P_P(reinterpret_cast<PGM_P>(list), find, separator);
}

inline static int stringlist_find_P_P(const __FlashStringHelper *list, const char *find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_find_P_P(reinterpret_cast<PGM_P>(list), find, separator);
}

inline static int stringlist_find_P_P(const __FlashStringHelper *list, const __FlashStringHelper *find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_find_P_P(reinterpret_cast<PGM_P>(list), reinterpret_cast<PGM_P>(find), separator);
}

inline static int stringlist_ifind_P_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR)
{
    if (!separator) {
        return -1;
    }
    if (!list || !find) {
        return -1;
    }
    const char separator_str[2] = { separator, 0 };
    return stringlist_ifind_P_P(list, find, separator_str);
}

inline static int stringlist_ifind_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_ifind_P_P(list, find, separator);
}

inline static int stringlist_ifind_P(const __FlashStringHelper *list, const char *find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_ifind_P_P(reinterpret_cast<PGM_P>(list), find, separator);
}

inline static int stringlist_ifind_P_P(const __FlashStringHelper *list, const char *find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_ifind_P_P(reinterpret_cast<PGM_P>(list), find, separator);
}

inline static int stringlist_ifind_P_P(const __FlashStringHelper *list, const __FlashStringHelper *find, char separator = STRINGLIST_SEPARATOR)
{
    return stringlist_ifind_P_P(reinterpret_cast<PGM_P>(list), reinterpret_cast<PGM_P>(find), separator);
}

// NOTE:
// string functions are nullptr safe
// only PGM_P is PROGMEM safe

// #define STRINGLIST_SEPARATOR                    ','
// #define STRLS                                   ","

// moved to misc_string.h

// find a string in a list of strings separated by a single characters
// -1 = not found, otherwise the number of the matching string
// int stringlist_find_P_P(PGM_P list, PGM_P find, char separator);

// multiple separators
// int stringlist_find_P_P(PGM_P list, PGM_P find, PGM_P separator);

// template<typename Tl, typename Tf, typename Tc>
// inline int stringlist_find_P_P(Tl list, Tf find, Tc separator) {
//     return stringlist_find_P_P(reinterpret_cast<PGM_P>(list), reinterpret_cast<PGM_P>(find), separator);
// }

bool str_endswith(const char *str, char ch);

#if defined(ESP8266)

bool str_endswith_P(PGM_P str, char ch);

#else

static inline bool str_endswith_P(PGM_P str, char ch) {
    return str_endswith(str, ch);
}

#endif

int strcmp_end(char *str1, size_t len1, const char *str2, size_t len2);
int strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2);
int strcmp_end_P_P(PGM_P str1, size_t len1, PGM_P str2, size_t len2);

inline static int strcmp_end(const char *str1, const char *str2) {
    return strcmp_end(const_cast<char *>(str1), strlen(str1), str2, strlen(str2));
}
inline static int strcmp_end_P(const char *str1, PGM_P str2) {
    return strcmp_end_P(str1, strlen(str1), str2, strlen_P(str2));
}

inline static int strcmp_end_P_P(PGM_P  str1, PGM_P str2) {
    return strcmp_end_P_P(str1, strlen_P(str1), str2, strlen_P(str2));
}

inline static int strcmp_end_P_P(PGM_P str1, size_t len1, PGM_P str2) {
    return strcmp_end_P_P(str1, len1, str2, strlen_P(str2));
}


// ends at maxLen characters or the first NUL byte
size_t str_replace(char *src, int from, int to, size_t maxLen = ~0);

// case insensitive comparision of from
size_t str_case_replace(char *src, int from, int to, size_t maxLen = ~0);

inline size_t String_replace(String &str, int from, int to)
{
    return str_replace(str.begin(), from, to, str.length());
}

inline size_t String_replaceIgnoreCase(String &str, int from, int to)
{
    return str_case_replace(str.begin(), from, to, str.length());
}

// trim trailing zeros
// return length of the string
// output can be nullptr to get the length
size_t printTrimmedDouble(Print *output, double value, int digits = 6);

// static inline bool String_startsWith(const String &str1, char ch) {
//     return str1.length() != 0 && str1.charAt(0) == ch;
// }
// bool String_endsWith(const String &str1, char ch);

// // compare functions that do not create a String object of "str2"
// bool String_startsWith(const String &str1, PGM_P str2);
// bool String_endsWith(const String &str1, PGM_P str2);

// bool String_equals(const String &str1, PGM_P str2);
// bool String_equalsIgnoreCase(const String &str1, PGM_P str2);

// use signed char to get an integer
template<typename T>
String enumToString(T value) {
    return String(static_cast<typename std::underlying_type<T>::type>(value));
}
