/**
Author: sascha_lammers@gmx.de
*/

#ifdef _WIN32

#pragma once

#define PSTR(str)                               str
#define FPSTR(str)                              (reinterpret_cast<const __FlashStringHelper *>(str))
#define snprintf_P snprintf
#define sprintf_P sprintf
#define strstr_P strstr
#define strlen_P strlen
#define vsnprintf_P vsnprintf
#ifndef pgm_read_byte
#define pgm_read_byte(a)                        (*(a))
#define pgm_read_byte_near(a)                   (*(a))
#endif
#define pgm_read_word(addr)                     (*reinterpret_cast<const uint16_t*>(addr))
#define pgm_read_dword(addr) 		            (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_ptr(addr)                      (*reinterpret_cast<const void* const *>(addr))
#define memcmp_P memcmp
#define strncmp_P strncmp
#define strcpy_P strcpy
#define strcat_P strcpy
#define strncpy_P strncpy
#define strncat_P strcpy
#define memcpy_P memcpy
#define memmove_P memmove

#ifndef PGM_P
typedef const char *PGM_P;
#endif
#ifndef PGM_VOID
typedef const void *PGM_VOID;
#endif
class __FlashStringHelper;

#define __FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#define __PSTR(s) ((const char *)s)
#define F(string_literal) (__FPSTR(__PSTR(string_literal)))

#define strcasecmp_P _stricmp
#define strncasecmp _strnicmp
#define strcmp_P strcmp

#endif
