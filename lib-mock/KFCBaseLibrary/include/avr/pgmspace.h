/**
Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#pragma once

#include <string>
#include <stdlib_noniso.h>

#ifndef PGM_P
#define PGM_P const char *
#endif

#define PSTR_ALIGN								4

#if 1
#define PSTR(str)                               (str)
#define PSTR1(str)								(str)
#define PSTRN(str, n)							(str)
#else
#define PSTR(str)                               (__register_flash_memory_string(str, PSTR_ALIGN))
#define PSTR1(str)								(__register_flash_memory_string(str, 1))
#define PSTRN(str, n)							(__register_flash_memory_string(str, n))
#endif

#define __PSTR(str)								(PSTR(str))
#undef FPSTR
#define FPSTR(str)                              (reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#define __FPSTR(str)							(reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#undef F
#define F(str)									(reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#define RFPSTR(str)								(reinterpret_cast<const char *>(PSTR(str)))

// add PROGMEM data
const void *__register_flash_memory(const void *ptr, size_t length, size_t alignment = PSTR_ALIGN);
// add PROGMEM string
const char *__register_flash_memory_string(const void *str, size_t alignment = PSTR_ALIGN);

// find PROGMEM string / pointer address (this is not strcmp or memcmp)
const char *__find_flash_memory_string(const char *str);

static inline const void *__find_flash_memory_string(const void *ptr) {
	return __find_flash_memory_string((const char *)ptr);
}

#define snprintf_P snprintf
#define sprintf_P sprintf

inline static char *strstr_P(char *str1, PGM_P str2, size_t len2 = ~0) {
	return strstr(str1, str2);
}

inline static const char *strstr_P(const char *str1, PGM_P str2, size_t len2 = ~0) {
	return strstr(str1, str2);
}

inline static size_t strlen_P(PGM_P str) {
	return strlen(str);
}

inline static int strncmp_P(const char *str1, PGM_P str2, size_t count) {
	return strncmp(str1, str2, count);
}

#define vsnprintf_P vsnprintf
#ifndef pgm_read_byte
#define pgm_read_byte(a)                        (*(a))
#define pgm_read_byte_near(a)                   (*(a))
#endif
#define pgm_read_word(addr)                     (*reinterpret_cast<const uint16_t*>(addr))
#define pgm_read_dword(addr) 		            (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_ptr(addr)                      (*reinterpret_cast<const void* const *>(addr))

inline static char *strcpy_P(char *dst, PGM_P src) {
	return strcpy(dst, src);
}

inline static char *strncpy_P(char *dst, PGM_P src, size_t count) {
	return strncpy(dst, src, count);
}

inline static char *strcat_P(char *dst, PGM_P src) {
	return strcat(dst, src);
}

inline static char *strcat_P(char *dst, PGM_P src, size_t count) {
	return strncat(dst, src, count);
}

#define memcmp_P memcmp
#define memcpy_P memcpy
#define memmove_P memmove

#ifndef PGM_P
#define PGM_P const char *
#endif

#ifndef PGM_VOID_P
#define PGM_VOID_P const void *
#endif
class __FlashStringHelper;

#define strcasecmp_P					_stricmp
#define strncasecmp						_strnicmp
#define strcmp_P						strcmp
#define strncasecmp_P					_strnicmp

#endif
