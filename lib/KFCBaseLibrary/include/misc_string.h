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

// internal
char *__strrstr(char *str, size_t stringLen, const char *find, size_t findLen);
char *__strrstr_P(char *str, size_t stringLen, PGM_P find, size_t findLen);
PGM_P __strrstr_P_P(PGM_P str, size_t stringLen, PGM_P find, size_t findLen);


#if defined(_MSC_VER)

static constexpr uint32_t SIZE_IRRELEVANT = 0x7fffffff;

#pragma push_macro("alloca")
#undef alloca
#define alloca _malloca

inline static void *memchr_P(void *s, int c, size_t n)
{
    return memchr(s, c, n);
}

inline static const void *memchr_P(const void *s, int c, size_t n)
{
    return memchr(s, c, n);
}

inline static void *memrchr(const void *s, int c, size_t n)
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

inline static const char *strrchr_P(const char *str, int c) {
    return strrchr(str, c);
}

inline static int strcasecmp(const char *str1, const char *str2) {
    return _stricmp(str1, str2);
}

inline static PGM_P strchr_P(PGM_P src, int c) {
    return strchr(src, c);
}

inline static char *strdup_P(PGM_P src) {
    return strdup(src);
}

#endif

#if defined(ESP8266) || defined(ESP32)

char *strdup_P(PGM_P src);

PGM_P strrchr_P(PGM_P str, int c);
PGM_P strchr_P(PGM_P str, int c);

#endif

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

// size == 0: returns ESZEROL
// str1 == nullptr: returns ESNULLP
// str2 == nullptr: returns -ESNULLP
// str1 == str2: returns 0 without comparing
int strncasecmp_P_P(PGM_P str1, PGM_P str2, size_t size);

// str or find == nullptr: returns nullptr
// strlen(find) == 0: returns str
PGM_P strstr_P_P(PGM_P str, PGM_P find, size_t findLen = 0);
PGM_P strcasestr_P_P(PGM_P str, PGM_P find, size_t findLen = 0);

// char *strrstr_P(char *str, PGM_P find, size_t findLen = 0);
char *strcasestr_P(char *str, PGM_P find, size_t findLen = 0);

inline static char *strcasestr_P(const char *str, PGM_P find, size_t findLen)
{
    return strcasestr_P(const_cast<char *>(str), find, findLen);
}

// using temporary and memrchr
inline static void *memrchr_P(PGM_VOID_P ptr, int ch, size_t n)
{
    if (!ptr) {
        return nullptr;
    }
    if (n == 0) {
        return const_cast<void *>(ptr);
    }
    else {
#if _MSC_VER
        auto tmp = reinterpret_cast<uint8_t *>(alloca(n));
#elif 0
        auto memPtr = std::unique_ptr<uint8_t[]>(new uint8_t[n]);
        auto tmp = memPtr.get();
#else
        uint8_t tmp[n];
#endif
        memcpy_P(tmp, ptr, n);
        return memrchr(tmp, ch, n);
    }
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



// using temporary and strncmp_P
inline static int strncmp_PP(PGM_P str1, PGM_P str2, size_t size)
{
    if (str1 == str2) {
        return 0;
    }
    else {
#if _MSC_VER
        auto tmp = reinterpret_cast<char *>(alloca(strlen_P(str1) + 1));
#elif 0
        auto ptr = std::unique_ptr<char[]>(new char[strlen_P(str1) + 1]);
        auto tmp = ptr.get();
#else
        char tmp[strlen_P(str1) + 1];
#endif
        strcpy_P(tmp, str1);
        return strncmp_P(tmp, str2, size);
    }
}

// using temporary and strncasecmp_P
inline static int strncasecmp_PP(PGM_P str1, PGM_P str2, size_t size)
{
    if (str1 == str2) {
        return 0;
    }
    else {
#if _MSC_VER
        auto tmp = reinterpret_cast<char *>(alloca(strlen_P(str1) + 1));
#elif 0
        auto ptr = std::unique_ptr<char []>(new char[strlen_P(str1) + 1]);
        auto tmp = ptr.get();
#else
        char tmp[strlen_P(str1) + 1];
#endif
        strcpy_P(tmp, str1);
        return strncasecmp_P(tmp, str2, size);
    }
}

#define strcmp_PP(str1, str2) strncmp_PP((str1), (str2), SIZE_IRRELEVANT)
#define strcasecmp_PP(str1, str2) strncasecmp_PP((str1), (str2), SIZE_IRRELEVANT)

// comparing PROGMEM directly
#define strcmp_P_P(str1, str2) strncmp_P_P((str1), (str2), SIZE_IRRELEVANT)
#define strcasecmp_P_P(str1, str2) strncasecmp_P_P((str1), (str2), SIZE_IRRELEVANT)

inline static char *stristr_P(const char *str1, PGM_P str2, size_t len2 = ~0) 
{
    return strcasestr_P(str1, str2, len2);
}

#define stringlist_casefind_P(list, find, separator) stringlist_ifind_P_P(list), (find), (separator))

char *strichr(char *, int ch);

inline static const char *strichr(const char *str1, int ch1)
{
    return strichr(const_cast<char *>(str1), ch1);
}

PGM_P strichr_P(PGM_P str1, int ch);

int strncmp_P_P(PGM_P str1, PGM_P str2, size_t len);
int strncasecmp_P_P(PGM_P str1, PGM_P str2, size_t size);

PGM_P strstr_P_P(PGM_P str, PGM_P find);
PGM_P strcasestr_P_P(PGM_P str, PGM_P find);

inline static char *strrstr(char *str, const char *find)
{
    if (!str || !find) {
        return nullptr;
    }
    if (!*find) {
        return str;
    }
    return __strrstr(str, strlen(str), find, strlen(find));
}

inline static const char *strrstr(const char *string, const char *find)
{
    return strrstr(const_cast<char *>(string), find);
}

inline static char *strrstr_P(char *str, PGM_P find)
{
    if (!str || !find) {
        return nullptr;
    }
    auto findLen = strlen_P(find);
    if (!findLen) {
        return str;
    }
    return __strrstr_P(str, strlen(str), find, findLen);
}

inline static char *strrstr_P(const char *str, PGM_P find)
{
    return strrstr_P(const_cast<char *>(str), find);
}

inline static PGM_P strrstr_P_P(PGM_P str, PGM_P find)
{
    if (!str || !find) {
        return nullptr;
    }
    auto findLen = strlen_P(find);
    if (!findLen) {
        return str;
    }
    return __strrstr_P_P(str, strlen_P(str), find, findLen);
}

#if __GNU_VISIBLE

inline static char *stristr(char *str1, const char *str2)
{
    return strcasestr(str1, str2);
}

#else

inline static char *stristr(char *str1, const char *str2, size_t len2 = ~0)
{
    return const_cast<char *>(strcasestr_P(str1, str2, len2));
}

#endif

#if defined(_MSC_VER)
#pragma pop_macro("alloca")
#endif
