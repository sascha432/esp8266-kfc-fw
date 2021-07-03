/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

// additional low level string functions

#ifdef _MSC_VER
#include <stdlib_noniso.h>
#else
#include <stdint.h>
#include <stdlib_noniso.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#endif
#include <pgmspace.h>
#include <memory>

// internal
char *__strrstr(char *str, size_t stringLen, const char *find, size_t findLen);
char *__strrstr_P(char *str, size_t stringLen, PGM_P find, size_t findLen);
PGM_P __strrstr_P_P(PGM_P str, size_t stringLen, PGM_P find, size_t findLen);

#if defined(_MSC_VER)

#ifndef __attribute__
#define __attribute__(...)
#endif

#ifndef PGM_VOID_P
#define PGM_VOID_P const void *
#endif

static constexpr uint32_t SIZE_IRRELEVANT = 0x7fffffff;

#pragma push_macro("alloca")
#undef alloca
#define alloca _malloca

inline void *memchr_P(void *s, int c, size_t n)
{
    return memchr(s, c, n);
}

inline const void *memchr_P(const void *s, int c, size_t n)
{
    return memchr(s, c, n);
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

inline const char *strrchr_P(const char *str, int c) {
    return strrchr(str, c);
}

inline int strcasecmp(const char *str1, const char *str2) {
    return _stricmp(str1, str2);
}

inline PGM_P strchr_P(PGM_P src, int c) {
    return strchr(src, c);
}

inline char *strdup_P(PGM_P src) {
    return strdup(src);
}

#endif

#if defined(ESP8266) || defined(ESP32)

char *strdup_P(PGM_P src);

PGM_P strrchr_P(PGM_P str, int c);
PGM_P strchr_P(PGM_P str, int c);

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

inline char *strcasestr_P(const char *str, PGM_P find, size_t findLen)
{
    return strcasestr_P(const_cast<char *>(str), find, findLen);
}

// using temporary and memrchr
inline void *memrchr_P(PGM_VOID_P ptr, int ch, size_t n)
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


// using temporary and strncmp_P
inline int strncmp_PP(PGM_P str1, PGM_P str2, size_t size)
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
inline int strncasecmp_PP(PGM_P str1, PGM_P str2, size_t size)
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

inline char *stristr_P(char *str1, PGM_P str2, size_t len2 = ~0)
{
    return strcasestr_P(str1, str2, len2);
}

#define stringlist_casefind_P(list, find, separator) stringlist_ifind_P_P(list), (find), (separator))

char *strichr(char *, int ch);

inline const char *strichr(const char *str1, int ch1)
{
    return strichr(const_cast<char *>(str1), ch1);
}

PGM_P strichr_P(PGM_P str1, int ch);

int strncmp_P_P(PGM_P str1, PGM_P str2, size_t len);
int strncasecmp_P_P(PGM_P str1, PGM_P str2, size_t size);

PGM_P strstr_P_P(PGM_P str, PGM_P find);
PGM_P strcasestr_P_P(PGM_P str, PGM_P find);

inline char *strrstr(char *str, const char *find)
{
    if (!str || !find) {
        return nullptr;
    }
    if (!*find) {
        return str;
    }
    return __strrstr(str, strlen(str), find, strlen(find));
}


extern "C" const char *strrstr(const char *string, const char *find);
// const char *strrstr(const char *string, const char *find)
// {
//     return strrstr(const_cast<char *>(string), find);
// }

inline char *strrstr_P(char *str, PGM_P find)
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

inline char *strrstr_P(const char *str, PGM_P find)
{
    return strrstr_P(const_cast<char *>(str), find);
}

inline PGM_P strrstr_P_P(PGM_P str, PGM_P find)
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

inline char *stristr(char *str1, const char *str2)
{
    return strcasestr(str1, str2);
}

#else

inline char *stristr(char *str1, const char *str2, size_t len2 = ~0)
{
    return const_cast<char *>(strcasestr_P(str1, str2, len2));
}

#endif

inline const char *stristr(const char *str1, const char *str2, size_t len2 = ~0)
{
    return stristr(str1, str2, len2);
}

#if defined(_MSC_VER)
#pragma pop_macro("alloca")
#endif
