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

#define STRINGLIST_SEPARATOR                    ','
#define STRLS                                   ","


#define ESNULLP                                 ( 400 ) /* null ptr */
#define ESZEROL                                 ( 401 ) /* length is zero */
#define EOK                                     ( 0 )

// internal
char *__strrstr(char *str, size_t stringLen, const char *find, size_t findLen);
char *__strrstr_P(char *str, size_t stringLen, PGM_P find, size_t findLen);
PGM_P __strrstr_P_P(PGM_P str, size_t stringLen, PGM_P find, size_t findLen);


// return index of the string in the list, 0 based
// all negative values are errors
// list == nullptr or find == nullptr: returns -ESNULLP
// separator == 0 or nullptr: returns -ESZEROL
int stringlist_find_P_P(PGM_P list, PGM_P find, PGM_P separator);
int stringlist_ifind_P_P(PGM_P list, PGM_P find, PGM_P separator);

// size == 0: returns ESZEROL
// str1 == nullptr: returns ESNULLP
// str2 == nullptr: returns -ESNULLP
// str1 == str2: returns 0 without comparing
int strncasecmp_P_P(PGM_P str1, PGM_P str2, size_t size);

// str or find == nullptr: returns nullptr
// strlen(find) == 0: returns str
PGM_P strstr_P_P(PGM_P str, PGM_P find);
PGM_P strcasestr_P_P(PGM_P str, PGM_P find);

// char *strrstr_P(char *str, PGM_P find);
char *strcasestr_P(char *str, PGM_P find);

inline static char *strcasestr_P(const char *str, PGM_P find)
{
    return strcasestr_P(const_cast<char *>(str), find);
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
        char tmp[n];
        memcpy_P(tmp, ptr, n);
        return memrchr(tmp, ch, n);
    }
}

inline static int stringlist_find_P_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR)
{
    if (!separator) {
        return -ESZEROL;
    }
    if (!list || !find) {
        return -ESNULLP;
    }
    const char separator_str[2] = { separator, 0 };
    return stringlist_find_P_P(list, find, separator_str);
}


inline static int stringlist_ifind_P_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR)
{
    if (!separator) {
        return -ESZEROL;
    }
    if (!list || !find) {
        return -ESNULLP;
    }
    const char separator_str[2] = { separator, 0 };
    return stringlist_ifind_P_P(list, find, separator_str);
}



// using temporary and strncmp_P
inline static int strncmp_PP(PGM_P str1, PGM_P str2, size_t size)
{
    if (str1 == str2) {
        return 0;
    }
    else {
        char tmp[strlen_P(str1) + 1];
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
        char tmp[strlen_P(str1) + 1];
        strcpy_P(tmp, str1);
        return strncasecmp_P(tmp, str2, size);
    }
}

#define strcmp_PP(str1, str2) strncmp_PP((str1), (str2), SIZE_IRRELEVANT)
#define strcasecmp_PP(str1, str2) strncasecmp_PP((str1), (str2), SIZE_IRRELEVANT)

// comparing PROGMEM directly
#define strcmp_P_P(str1, str2) strncmp_P_P((str1), (str2), SIZE_IRRELEVANT)
#define strcasecmp_P_P(str1, str2) strncasecmp_P_P((str1), (str2), SIZE_IRRELEVANT)

#define stristr_P(str1, str2) strcasestr_P((str1), (str2))

#define stringlist_find_P(list, find, separator) stringlist_find_P_P(list), (find), (separator))
#define stringlist_ifind_P(list, find, separator) stringlist_ifind_P_P(list), (find), (separator))
#define stringlist_casefind_P(list, find, separator) stringlist_ifind_P_P(list), (find), (separator))



#if defined(_MSC_VER)

inline static void *memchr_P(void *s, int c, size_t n) {
    return memchr(s, c, n);
}

inline static const void *memchr_P(const void *s, int c, size_t n) {
    return memchr(s, c, n);
}

inline static void *memrchr(const void *s, int c, size_t n) {
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

#else

char *strdup_P(PGM_P src);


inline static const char *strchr_P(const char *str, int c) {
    // PROGMEM safe
    return strchr(str, c);
}

inline static const char *strrchr_P(const char *str, int c) {
    // PROGMEM safe
    return strrchr(str, c);
}

#endif

char *strichr(char *, char ch);

inline static const char *strichr(const char *str1, char ch1)
{
    return strichr(const_cast<char *>(str1), ch1);
}

PGM_P strichr_P(PGM_P str1, char ch);

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
    if (!str || !str) {
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

inline static char *stristr(char *str1, const char *str2)
{
    return const_cast<char *>(strcasestr_P(str1, str2));
}

#endif

