/**
Author: sascha_lammers@gmx.de
*/

#pragma once

//#define F(str) (reinterpret_cast<char *>(str))
#define PSTR(str) str
#define FPSTR(str) str
#define snprintf_P snprintf
#define sprintf_P sprintf
#define strstr_P strstr
#define strlen_P strlen
#ifndef pgm_read_byte
#define pgm_read_byte(a)                        (*a)
#endif
#define pgm_read_word(addr)                     (*reinterpret_cast<const uint16_t*>(addr))
#define pgm_read_dword(addr) 		            (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_ptr(addr)                      (*reinterpret_cast<const void* const *>(addr))
#define memcmp_P memcmp
#define strcpy_P strcpy
#define strncpy_P strncpy
#define memcpy_P memcpy

int constexpr constexpr_strlen(const char* str) {
    return *str ? 1 + constexpr_strlen(str + 1) : 0;
}

#define constexpr_strlen_P constexpr_strlen

#ifndef PGM_P
typedef const char *PGM_P;
#endif
class __FlashStringHelper;

#define __FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#define __PSTR(s) ((const char *)s)
#define F(string_literal) (__FPSTR(__PSTR(string_literal)))

#define strcasecmp_P _stricmp
#define strncasecmp _strnicmp
#define strcmp_P strcmp
