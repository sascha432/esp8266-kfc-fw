/**
Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#pragma once

#define PSTR_ALIGN								4

#define PSTR(str)                               (__register_flash_memory_string(str, PSTR_ALIGN))
#define __PSTR(str)								(PSTR(str))
#define FPSTR(str)                              (reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#define __FPSTR(str)							(reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#define F(str)									(reinterpret_cast<const __FlashStringHelper *>(PSTR(str)))
#define RFPSTR(str)								(reinterpret_cast<const char *>(PSTR(str)))
#define PSTR1(str)								(__register_flash_memory_string(str, 1))
#define PSTRN(str, n)							(__register_flash_memory_string(str, n))

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

#define strcasecmp_P					_stricmp
#define strncasecmp						_strnicmp
#define strcmp_P						strcmp
#define strncasecmp_P						_strnicmp

#endif
