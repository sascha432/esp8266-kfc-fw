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

#endif

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
