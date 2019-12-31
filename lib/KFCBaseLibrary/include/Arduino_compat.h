/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Quick and dirty library to compile ESP8266 and ESP32 code with MSVC++ as native Win32 Console Application
 */

#pragma once

#if _DEBUG && _MSC_VER
#define _debug_new new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define _debug_new new
#endif

#if defined(ESP32)

#include <Arduino.h>
#include <global.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <FS.h>

#include "esp32_compat.h"

#define PROGMEM_STRING_DECL(name)               extern const char _shared_progmem_string_##name[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)         const char _shared_progmem_string_##name[] PROGMEM = { value };

class __FlashStringHelper;

#define SPGM(name)                              _shared_progmem_string_##name
#define FSPGM(name)                             reinterpret_cast<const __FlashStringHelper *>(SPGM(name))
#define PSPGM(name)                             (PGM_P)(SPGM(name))

#define constexpr_strlen                        strlen
#define constexpr_strlen_P                      strlen_P

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

#define PROGMEM_STRING_DECL(name)               extern const char _shared_progmem_string_##name[] PROGMEM;
#define PROGMEM_STRING_DEF(name, value)         const char _shared_progmem_string_##name[] PROGMEM = { value };

#define SPGM(name)                              _shared_progmem_string_##name
#define FSPGM(name)                             FPSTR(SPGM(name))
#define PSPGM(name)                             (PGM_P)(SPGM(name))

// #define constexpr_strlen                        strlen
// #define constexpr_strlen_P                      strlen_P

int constexpr constexpr_strlen(const char* str) {
    return *str ? 1 + constexpr_strlen(str + 1) : 0;
}

#define constexpr_strlen_P constexpr_strlen


#include "debug_helper.h"
#include "misc.h"

#elif _WIN32 || _WIN64

#define NOMINMAX
#define _CRTDBG_MAP_ALLOC

#include <stdint.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <string.h>
#include <time.h>
#include <winsock.h>
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

#define PROGMEM_STRING_DECL(name)               extern const char * const _shared_progmem_string_##name;
#define PROGMEM_STRING_DEF(name, value)         const char * const _shared_progmem_string_##name = value;

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

#else

#error Platform not supported

#endif

#ifndef _STRINGIFY
#define _STRINGIFY(s)                   __STRINGIFY(s)
#endif
#ifndef __STRINGIFY
#define __STRINGIFY(s)                  #s
#endif
