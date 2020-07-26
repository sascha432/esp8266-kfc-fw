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

#define PROGMEM_STRING_DECL(name)                       extern const char PROGMEM_STRING_ID(name)[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)                 const char PROGMEM_STRING_ID(name)[] PROGMEM = { value };

#if defined(ESP32)

#include <Arduino.h>
#include <global.h>
#include <WiFi.h>
#include <FS.h>

#include "esp32_compat.h"

class __FlashStringHelper;

#define SPGM(name, ...)                                 PROGMEM_STRING_ID(name)
#define FSPGM(name, ...)                                reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#define PSPGM(name, ...)                                (PGM_P)(SPGM(name))

#include "debug_helper.h"
#include "misc.h"

#elif defined(ESP8266)

#include <Arduino.h>
#include <global.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <WiFiUdp.h>
#include <FS.h>

#include "esp8266_compat.h"

#define SPGM(name, ...)                                 PROGMEM_STRING_ID(name)
#define FSPGM(name, ...)                                FPSTR(SPGM(name))
#define PSPGM(name, ...)                                (PGM_P)(SPGM(name))

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
#include <CRTDBG.h>

#define __attribute__(a)

#ifndef DEBUG_OUTPUT
#define DEBUG_OUTPUT Serial
#endif

#include <global.h>

void panic();
void yield();

void init_winsock();

uint64_t micros64();

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
#include "debug_helper_enable.h"
#include "misc.h"

extern "C" uint32_t _EEPROM_start;

inline void noInterrupts() {
}
inline void interrupts() {
}

extern const String emptyString;

#else

#error Platform not supported

#endif

#include "FileOpenMode.h"
#include "constexpr_tools.h"

#define __CLASS_FROM__(name)             StringConstExpr::StringArray<DebugHelperConstExpr::get_class_name_len(name)>(DebugHelperConstExpr::get_class_name_start(name)).array

#ifndef __CLASS__
#ifdef _MSC_VER
#define __CLASS__                       __CLASS_FROM__(__FUNCSIG__)
#else
// __PRETTY_FUNCTION__ is not constexpr, we need to parse it during runtime
// extern String __class_from_String(const char *str);
#define __CLASS__                       __PRETTY_FUNCTION__ //__class_from_String(__PRETTY_FUNCTION__).c_str()
#endif
#endif

// support nullptr as zero start_length
static size_t constexpr constexpr_strlen(const char *str)
{
    return StringConstExpr::strlen(str);
}

static size_t constexpr constexpr_strlen(const uint8_t *str)
{
    return StringConstExpr::strlen(str);
}

static size_t constexpr constexpr_strlen_P(const char *str)
{
    return StringConstExpr::strlen(str);
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

#define __VA_ARGS_COUNT__(...)              __va_args__::va_count(__VA_ARGS__)
