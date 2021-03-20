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

inline static void *memrchr_P(const void *s, int c, size_t n) {
    return memrchr(s, c, n);
}

#else

void *memrchr_P(const void *s, int c, size_t n);
PGM_P strchr_P(PGM_P str, int c);
PGM_P strrchr_P(PGM_P str, int c);
char *strdup_P(PGM_P src);

#endif

inline static char *strichr(char *str1, char ch)
{
    if (!str1) {
        return nullptr;
    }
    if (!ch) {
        // special case: find end of string
        return str1 + strlen(str1);
    }
    ch = tolower(ch);
    if (toupper(ch) == ch) {
        // special case: lower and upercase are the same
        return strchr(str1, ch);
    }
    while(*str1 && tolower(*str1) != ch) {
        str1++;
    }
    return (*str1 == 0) ? nullptr : str1;
}

inline static const char *strichr(const char *str1, char ch1)
{
    return strichr(const_cast<char *>(str1), ch1);
}

inline static const char *strichr_P(const char *str1, char ch)
{
    if (!str1) {
        return nullptr;
    }
    if (!ch) {
        // special case find end of string
        return str1 + strlen_P(str1);
    }
    ch = tolower(ch);
    if (toupper(ch) == ch) {
        // special case: lower and upercase are the same
        return strchr_P(str1, ch);
    }
    char ch2;
    while(((ch2 = pgm_read_byte(str1)) != 0) && tolower(ch2) != ch) {
        str1++;
    }
    return (ch2 == 0) ? nullptr : str1;
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
inline const char *__strrstr_P_P(const char *string, size_t stringLen, const char *find, size_t findLen);

inline const char *strrstr_P_P(const char *string, const char *find);
inline char *__strrstr(char *string, size_t stringLen, const char *find, size_t findLen);
inline const char *__strrstr(const char *string, size_t stringLen, const char *find, size_t findLen) {
    return __strrstr_P_P(string, stringLen, find, findLen);
}
inline char *__strrstr_P(char *string, size_t stringLen, const char *find, size_t findLen);
inline const char *__strrstr_P(const char *string, size_t stringLen, const char *find, size_t findLen) {
    return __strrstr_P_P(string, stringLen, find, findLen);
}
inline const char *__strrstr_P_P(const char *string, size_t stringLen, const char *find, size_t findLen);


inline static char *stristr(char *str1, const char *str2) {
    return const_cast<char *>(strcasestr_P_P(str1, str2)); //TODO this this should be stristr or strcasestr. PGM_P not allowed
}

inline static char *stristr_P(char *str1, const char *str2) {
    return const_cast<char *>(strcasestr_P_P(str1, str2)); //TODO this this should be stristr_P or strcasestr_P, str1 cannot be PGM_P
}

inline static const char *stristr_P(const char *str1, const char *str2) {
    return strcasestr_P_P(str1, str2);
}
