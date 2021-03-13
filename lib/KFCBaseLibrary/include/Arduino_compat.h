/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Quick and dirty library to compile ESP8266 and ESP32 code with MSVC++ as native Win32 Console Application
 */

#pragma once

#if DEBUG && _MSC_VER
#ifndef _DEBUG
#error _DEBUG required
#endif

#if ARDUINO <= 100
#error ARDUINO>100 required
#endif

#if !UNICODE || !_UNICODE
#error UNICODE and _UNICODE required
#endif

#if !_CRT_SECURE_NO_WARNINGS
#error _CRT_SECURE_NO_WARNINGS required
#endif

#if _CRTDBG_MAP_ALLOC
#define new                                             new( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define CHECK_MEMORY(...)                               if (_CrtCheckMemory() == false) { __debugbreak(); }
#else
#define CHECK_MEMORY(...)                               ;
//#warning _CRTDBG_MAP_ALLOC=0
#endif

#else
#define CHECK_MEMORY(...)                               ;
#endif

#if _MSC_VER

#ifndef __attribute__
#define __attribute__(...)
#endif

// place on heap for memory check
#define stack_array(name, type, size)                   size_t name##_unique_ptr_size = sizeof(type[size]); auto name##_unique_ptr = std::unique_ptr<type[]>(new type[size]); auto *name = (name##_unique_ptr).get()
#define sizeof_stack_array(name)                        name##_unique_ptr_size
#else
#define stack_array(name, type, size)                   auto name[size]
#define sizeof_stack_array(name)                        sizeof(name)
#endif

//
// NOTE:
// if a flash string is not defined, run
// pio run -t buildspgm
// to rebuild the string database
//
#define PROGMEM_STRING_ID(name)                         SPGM_##name

#if _MSC_VER

#define PROGMEM_STRING_DECL(name)                       extern const char *PROGMEM_STRING_ID(name) PROGMEM;
#define PROGMEM_STRING_DEF(name, value)                 const char *PROGMEM_STRING_ID(name) PROGMEM = (const char *)__register_flash_memory(value, constexpr_strlen(value) + 1, PSTR_ALIGN);

#else

#define PROGMEM_STRING_DECL(name)                       extern const char PROGMEM_STRING_ID(name)[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM;
#define PROGMEM_STRING_DEF(name, value)                 const char PROGMEM_STRING_ID(name)[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = { value };

#endif

#if defined(ESP32)

#include <Arduino.h>
#include <global.h>
#include <WiFi.h>
#include <FS.h>

#include "esp32_compat.h"

class __FlashStringHelper;

#define KFCFS                                           SPIFFS
#define KFCFS_MAX_FILE_LEN                              31
// includes directory slashes and filename
#define KFCFS_MAX_PATH_LEN                              KFCFS_MAX_FILE_LEN

#define SPGM(name, ...)                                 PROGMEM_STRING_ID(name)
#define FSPGM(name, ...)                                reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#define PSPGM(name, ...)                                (PGM_P)(SPGM(name))

#ifndef __attribute__packed__
#define __attribute__packed__                           __attribute__((packed))
#define __attribute__unaligned__                        __attribute__((__aligned__(1)))
#define PSTR1(str)                                      PSTRN(str, 1)
#endif

#include "debug_helper.h"
#include "misc.h"

#elif defined(ESP8266)

#include <Arduino.h>
#include <global.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <WiFiUdp.h>
#if USE_LITTLEFS
#include <LittleFS.h>
#define KFCFS                                           LittleFS
#define KFCFS_MAX_FILE_LEN                              31
#define KFCFS_MAX_PATH_LEN                              127
#else
#include <FS.h>
#define KFCFS                                           SPIFFS
#define KFCFS_MAX_FILE_LEN                              31
// includes directory slashes and filename
#define KFCFS_MAX_PATH_LEN                              KFCFS_MAX_FILE_LEN
#endif

#include "esp8266_compat.h"

#define SPGM(name, ...)                                 PROGMEM_STRING_ID(name)
#define FSPGM(name, ...)                                FPSTR(SPGM(name))
#define PSPGM(name, ...)                                (PGM_P)(SPGM(name))

#ifndef __attribute__packed__
#define __attribute_packed__                            __attribute__((packed))
#define __attribute__packed__                           __attribute__((packed))
#define __attribute__unaligned__                        __attribute__((__aligned__(1)))
#define PSTR1(str)                                      PSTRN(str, 1)
#endif

#include "debug_helper.h"
#include "misc.h"

#elif _MSC_VER

#define NOMINMAX
#if !defined(_CRTDBG_MAP_ALLOC) && DEBUG
#define _CRTDBG_MAP_ALLOC
#endif

#include <stdint.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <strsafe.h>
#include <vector>
#include <iostream>
#include <Psapi.h>
#include <assert.h>
#include <CRTDBG.h>
#include <pgmspace.h>

#include "WString.h"

#define KFCFS                                           SPIFFS
#define KFCFS_MAX_FILE_LEN                              31
// includes directory slashes and filename
#define KFCFS_MAX_PATH_LEN                              KFCFS_MAX_FILE_LEN

#include <ets_sys_win32.h>
#include <ets_timer_win32.h>

#define __attribute__(a)

#ifndef DEBUG_OUTPUT
#define DEBUG_OUTPUT Serial
#endif

#include <global.h>

void init_winsock();

#ifndef strdup
#define strdup _strdup
#endif

uint16_t __builtin_bswap16(uint16_t);

#include "Arduino.h"

#define PROGMEM

#define SPGM(name, ...)                                 PROGMEM_STRING_ID(name)
#define FSPGM(name, ...)                                FPSTR(SPGM(name))
#define PSPGM(name, ...)                                (PGM_P)(SPGM(name))

#include <pgmspace.h>

void throwException(PGM_P message);

#include "WString.h"
#include "Print.h"
#include "Stream.h"
#include "FS.h"
#include "Serial.h"
#include "WiFi.h"
#include "WiFiUDP.h"
#include "ESP.h"

#include "debug_helper.h"
#include "misc.h"

extern "C" uint32_t _EEPROM_start;

extern const String emptyString;

#else

#error Platform not supported

#endif

#include "FileOpenMode.h"
#include "constexpr_tools.h"
#include <stl_ext/utility.h>

#if __GNUC__

static size_t constexpr constexpr_strlen(const char *str) noexcept
{
    return __builtin_strlen(str);
}

#else

constexpr size_t constexpr_strlen(const char *s) noexcept
{
    return s ? (*s ? 1 + constexpr_strlen(s + 1) : 0) : 0;
}

#endif

static size_t constexpr constexpr_strlen(const uint8_t *str) noexcept
{
    return constexpr_strlen((const char *)str);
}

static size_t constexpr constexpr_strlen_P(const char *str) noexcept
{
    return constexpr_strlen(str);
}

static size_t constexpr constexpr_strlen(const __FlashStringHelper *str) noexcept
{
    return constexpr_strlen((const char *)str);
}


#ifndef _STRINGIFY
#define _STRINGIFY(...)                     ___STRINGIFY(__VA_ARGS__)
#endif
#define ___STRINGIFY(...)                   #__VA_ARGS__

// reinterpret_cast FPSTR
#ifndef RFPSTR
#define RFPSTR(str)                         reinterpret_cast<PGM_P>(str)
#endif

// equivalent to __FlashStringHelper/const char *
// __FlashBufferHelper/const uint8_t *
class __FlashBufferHelper;

namespace __va_args__
{
    template<typename ...Args>
    constexpr std::size_t va_count(Args &&...) { return sizeof...(Args); }
}

#define __VA_ARGS_COUNT__(...)                          __va_args__::va_count(__VA_ARGS__)

#if FLASH_STRINGS_AUTO_INIT
#define AUTO_STRING_DEF(...)                            AUTO_INIT_SPGM(__VA_ARGS__),
#define FLASH_STRING_GENERATOR_AUTO_INIT(...) \
    static bool __flash_string_generator_auto_init_var = []() { \
        SPGM_P strings[] = { \
            __VA_ARGS__ nullptr \
        };
        return true; \
    }
#else

#define AUTO_STRING_DEF(name, ...)                      PROGMEM_STRING_DECL(name);
#define FLASH_STRING_GENERATOR_AUTO_INIT(...)           __VA_ARGS__
#endif

extern "C" void __dump_binary(const void *ptr, size_t len, size_t perLine, PGM_P title = nullptr);
extern "C" void __dump_binary_to(Print &output, const void *ptr, size_t len, size_t perLine, PGM_P title = nullptr);

#if _MSC_VER
#include "../../../include/spgm_auto_strings.h"
#elif defined(HAVE_FLASH_STRING_GENERATOR) && HAVE_FLASH_STRING_GENERATOR
#include "spgm_auto_strings.h"
#include "spgm_auto_def.h"
#endif
