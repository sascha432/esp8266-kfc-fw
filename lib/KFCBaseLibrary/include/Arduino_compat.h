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
#if !_CRTDBG_MAP_ALLOC
#error _CRTDBG_MAP_ALLOC required
#endif
#define new                                             new( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define CHECK_MEMORY(...)                               if (_CrtCheckMemory() == false) { __debugbreak(); }
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

#define PROGMEM_STRING_DECL(name)               extern const char _shared_progmem_string_##name[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)         const char _shared_progmem_string_##name[] PROGMEM = { value };

#if defined(ESP32)

#include <Arduino.h>
#include <global.h>
#include <WiFi.h>
#include <FS.h>

#include "esp32_compat.h"

class __FlashStringHelper;

#define SPGM(name)                              _shared_progmem_string_##name
#define FSPGM(name)                             reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#define PSPGM(name)                             (PGM_P)(SPGM(name))

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

#define SPGM(name)                              _shared_progmem_string_##name
#define FSPGM(name)                             FPSTR(SPGM(name))
#define PSPGM(name)                             (PGM_P)(SPGM(name))

#include "debug_helper.h"
#include "misc.h"

#elif _WIN32 || _WIN64

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

#ifndef strdup
#define strdup _strdup
#endif

uint16_t __builtin_bswap16(uint16_t);

#include "Arduino.h"

#define PROGMEM

#define SPGM(name)                              _shared_progmem_string_##name
#define FSPGM(name)                             FPSTR(SPGM(name))
#define PSPGM(name)                             (PGM_P)(SPGM(name))

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

namespace fs {
    class FileOpenMode {
    public:
        static const char *read;
        static const char *write;
        static const char *append;
    };
};

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
size_t constexpr constexpr_strlen(const char *str)
{
    return StringConstExpr::strlen(str);
}

size_t constexpr constexpr_strlen_P(const char *str)
{
    return StringConstExpr::strlen(str);
}

#ifndef _STRINGIFY
#define _STRINGIFY(...)                  ___STRINGIFY(__VA_ARGS__)
#endif
#define ___STRINGIFY(...)                #__VA_ARGS__

// reinterpret_cast FPSTR
#ifndef RFPSTR
#define RFPSTR(str)                      reinterpret_cast<PGM_P>(str)
#endif

// equivalent to __FlashStringHelper/const char *
// __FlashBufferHelper/const uint8_t *
class __FlashBufferHelper;
