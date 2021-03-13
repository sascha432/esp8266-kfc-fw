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
