/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>


#if DEBUG

void ___debugbreak_and_panic(const char* filename, int line, const char* function);

// invokes panic() on ESP8266/32, calls __debugbreak() with visual studio to intercept and debug the error
#define __debugbreak_and_panic()                        ___debugbreak_and_panic(DebugHelper::basename(__FILE__), __LINE__, __FUNCTION__)
#define __debugbreak_and_panic_printf_P(fmt, ...)       DEBUG_OUTPUT.printf_P(fmt, ## __VA_ARGS__); __debugbreak_and_panic();

// call in setup, after initializing the output stream
#define DEBUG_HELPER_INIT()                             DebugHelper::__state = DEBUG_HELPER_STATE_DEFAULT;
#define DEBUG_HELPER_SILENT()                           DebugHelper::__state = DEBUG_HELPER_STATE_DISABLED;

#define debug_helper_set_src()                          { DebugHelper::__file = DebugHelper::basename(__FILE__); DebugHelper::__line = __LINE__; __function = __FUNCTION__; }

#ifndef DEBUG_HELPER_STATE_DEFAULT
#define DEBUG_HELPER_STATE_DEFAULT                      DEBUG_HELPER_STATE_ACTIVE
#endif

#define DEBUG_HELPER_STATE_ACTIVE                       0
#define DEBUG_HELPER_STATE_FILTERED                     1
#define DEBUG_HELPER_STATE_DISABLED                     2

typedef enum {
    DEBUG_FILTER_INCLUDE,
    DEBUG_FILTER_EXCLUDE
} DebugFilterTypeEnum_t;

typedef struct {
    DebugFilterTypeEnum_t type;
    String pattern;
} DebugHelperFilter_t;

typedef std::vector<DebugHelperFilter_t> DebugHelperFilterVector;

class DebugHelper {
public:
    static String __file;
    static unsigned __line;
    static String __function;

    static const char *basename(const String &file);

    static uint8_t __state;
    static DebugHelperFilterVector __filters;

};

extern String _debugPrefix;

template<typename T>
T _debug_helper_print_result_P(const char *file, int line, const char *function, T result, PGM_P format, ...) {
    if (DebugHelper::__state != DEBUG_HELPER_STATE_ACTIVE) {
        return result;
    }
#if DEBUG_INCLUDE_SOURCE_INFO
    DEBUG_OUTPUT.printf(_debugPrefix.c_str(), millis(), file, line, ESP.getFreeHeap(), function);
#else
    DEBUG_OUTPUT.print(_debugPrefix);
#endif
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char *buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = _debug_new char[len + 1];
        if (!buffer) {
            return result;
        }
        va_start(arg, format);
        len = vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    DEBUG_OUTPUT.write((const uint8_t *)buffer, len);
    if (buffer != temp) {
        delete[] buffer;
    }
    if (std::is_same<T, char>::value) {
        DEBUG_OUTPUT.printf("%d (%x, '%c')\n", result, result, isprint(result) ? result : '.');
    } else if (std::is_same<T, int>::value) {
        DEBUG_OUTPUT.printf("%d (%x)\n", result, result);
    } else if (std::is_same<T, long>::value) {
        DEBUG_OUTPUT.printf("%ld (%lx)\n", (long)result, (long)result);
    } else {
        DEBUG_OUTPUT.println(String(result));
    }
    return result;
}

#if DEBUG_INCLUDE_SOURCE_INFO
    #define __debug_prefix(out)                 out.printf(_debugPrefix.c_str(), millis(), DebugHelper::basename(__FILE__), __LINE__, ESP.getFreeHeap(), __FUNCTION__);
#else
    #define __debug_prefix(out)                 out.print(_debugPrefix);
#endif
#define debug_prefix()                          __debug_prefix(DEBUG_OUTPUT)

#define debug_print(msg)                        if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { DEBUG_OUTPUT.print(msg); }
#define debug_println(msg)                      if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { debug_prefix(); DEBUG_OUTPUT.println(msg); }
#define debug_printf(fmt, ...)                  if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { debug_prefix(); DEBUG_OUTPUT.printf(fmt, ## __VA_ARGS__); }
#define debug_printf_P(fmt, ...)                if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { debug_prefix(); DEBUG_OUTPUT.printf_P(fmt, ## __VA_ARGS__); }

#define debug_print_result_P(result, fmt)       _debug_helper_print_result_P(DebugHelper::basename(__FILE__), __LINE__, __FUNCTION__, result, fmt)
#define debug_call_P(function, fmt, ...)        _debug_helper_print_result_P(DebugHelper::basename(__FILE__), __LINE__, __FUNCTION__, function(__VA_ARGS__), fmt, __VA_ARGS__)

#define IF_DEBUG(...)                           __VA_ARGS__
#define debug_call(name, ...)                   DEBUG_OUTPUT.printf("%s:%d %s\n", DebugHelper::basename(__FILE__), __LINE__, _______STR(name)); name ## __VA_ARGS__

#if DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_SOURCE_ARGS                       const char *_debug_filename, int _debug_lineno, const char *_debug_function,
#define DEBUG_SOURCE_ADD_ARGS                   DebugHelper::basename(__FILE__), __LINE__, __FUNCTION__,
#define DEBUG_SOURCE_PASS_ARGS                  _debug_filename, _debug_lineno, _debug_function,
#define DEBUG_SOURCE_APPEND_ARGS                , _debug_filename, _debug_lineno, _debug_function
#define DEBUG_SOURCE_FORMAT                     " (%s:%u@%s)"
#else
#define DEBUG_SOURCE_ARGS
#define DEBUG_SOURCE_ADD_ARGS
#define DEBUG_SOURCE_PASS_ARGS
#define DEBUG_SOURCE_APPEND_ARGS
#define DEBUG_SOURCE_FORMAT
#endif

// 3FFE8000h	dram0	14000h	RAM	RW	User data RAM. Available to applications.
// 3FFFC000h		4000h	RAM		ETS system data RAM.
// 40100000h	iram1	8000h	RAM	RW	Instruction RAM. Used by bootloader to load SPI Flash <40000h.
// 40108000h	?	4000h	RAM	RW	Instruction RAM/FLASH cache ram. OTA bootloader uses it. Mapped here if bit 4 (bit mask 0x10) is clear in the dport0 register 0x3ff00024
// 4010C000h	?	4000h	RAM	RW	Instruction RAM/FLASH cache ram. Mapped here if bit 3 (bit mask 0x8) is clear in the dport0 register 0x3ff00024
#define DEBUG_CHECK_C_STR(str) ((uint32_t)(void *)str)>=0x3fff0000 ? str : "INVALID_PTR"
#define DEBUG_CHECK_P_STR(str) ((uint32_t)(void *)str)<0x40108000 ? str : SPGM(invalid_flash_ptr)

#else

#define DEBUG_HELPER_INIT()         ;
#define DEBUG_HELPER_SILENT()       ;

#define __debugbreak_and_panic()                        panic();
#define __debugbreak_and_panic_printf_P(fmt, ...)       Serial.printf_P(fmt, __VA_ARGS__); panic();

#define debug_print(...)            ;
#define debug_println(...)          ;
#define debug_printf(...)           ;
#define debug_printf_P(...)         ;
#define debug_wifi_diag()           ;
#define debug_prefix(...)           ;

#define debug_print_result_P(result, fmt)       result
#define debug_call_P(function, fmt, ...)        function(__VA_ARGS__)

#define debug_helper_set_src()          ;

#define IF_DEBUG(...)

#define DEBUG_SOURCE_ARGS
#define DEBUG_SOURCE_ADD_ARGS
#define DEBUG_SOURCE_PASS_ARGS
#define DEBUG_SOURCE_APPEND_ARGS
#define DEBUG_SOURCE_FORMAT

#define DEBUG_CHECK_C_STR(str) str
#define DEBUG_CHECK_P_STR(str) str

#endif

#include "debug_helper_enable.h"
