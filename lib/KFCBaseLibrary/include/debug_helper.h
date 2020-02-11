/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <Print.h>
#include <vector>
#include "constexpr_tools.h"

#if 1
#define __BASENAME_FILE__                               StringConstExpr::basename(__FILE__)
#endif

#if defined(_MSC_VER)
#define ____DEBUG_FUNCTION__                              __FUNCSIG__
#else
#define ____DEBUG_FUNCTION__                              __FUNCTION__
#endif

#if 1
#define __DEBUG_FUNCTION__                              DebugHelper::pretty_function(____DEBUG_FUNCTION__)
#else
#define __DEBUG_FUNCTION__                              ____DEBUG_FUNCTION__
#endif

#if DEBUG

void ___debugbreak_and_panic(const char* filename, int line, const char* function);

extern const char ___debugPrefix[] PROGMEM;

// invokes panic() on ESP8266/32, calls __debugbreak() with visual studio to intercept and debug the error
#define __debugbreak_and_panic()                        ___debugbreak_and_panic(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)
#define __debugbreak_and_panic_printf_P(fmt, ...)       DEBUG_OUTPUT.printf_P(fmt, ## __VA_ARGS__); __debugbreak_and_panic();

// call in setup, after initializing the output stream
#define DEBUG_HELPER_INIT()                             DebugHelper::__state = DEBUG_HELPER_STATE_DEFAULT;
#define DEBUG_HELPER_SILENT()                           DebugHelper::__state = DEBUG_HELPER_STATE_DISABLED;

#define debug_helper_set_src()                          { DebugHelper::__file = __BASENAME_FILE__; DebugHelper::__line = __LINE__; __function = __DEBUG_FUNCTION__; }

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

//#include <FixedCircularBuffer.h>

class DebugHelper {
public:
    static String __file;
    static unsigned __line;
    static String __function;

    static uint8_t __state;
    static DebugHelperFilterVector __filters;

    typedef struct {
        const char *file;
        int line;
        uint32_t time;
    } Positon_t;

    // static FixedCircularBuffer<Positon_t,100> __pos;
    // static void reg(const char *file, int line) {
    //     if (!__pos.isLocked()) {
    //         __pos.push_back(Positon_t({file,line,micros()}));
    //     }
    // }
    // static void regPrint(FixedCircularBuffer<Positon_t,100>::iterator it);
    static const char* pretty_function(const char* name);
    static void activate(bool state = true) {
        __state = state ? DEBUG_HELPER_STATE_ACTIVE : DEBUG_HELPER_STATE_DISABLED;
    }
};


#include "DebugHelperPrintValue.h"

#if DEBUG_INCLUDE_SOURCE_INFO

#define DEBUG_HELPER_POSITION                           DebugHelperPosition(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)

class DebugHelperPosition : public DebugHelperPrintValue {
public:
    DebugHelperPosition(const char* file, int line, const char* functionName) : _file(file), _line(line), _functionName(functionName) {
    }

    virtual void printPrefix() {
        printf_P(___debugPrefix, millis(), _file, _line, ESP.getFreeHeap(), _functionName);
    }

    const char* _file;
    int _line;
    const char* _functionName;
};

#else

#define DEBUG_HELPER_POSITION                           DebugHelperPosition()

class DebugHelperPosition : public DebugHelperPrintValue {
public:
    DebugHelperPosition() {
    }

    virtual void printPrefix() {
            DEBUG_OUTPUT.print(FPSTR(___debugPrefix));
    }
};

#endif

// #define __debug_prefix(out)                 out.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), __DEBUG_FUNCTION__ );

#if DEBUG_INCLUDE_SOURCE_INFO
    #define __debug_prefix(out)                 out.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), __DEBUG_FUNCTION__ );
    // #define __debug_prefix(out)                 out.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), __DEBUG_FUNCTION__ );
#else
    #define __debug_prefix(out)                 out.print(FPSTR(___debugPrefix));
#endif
#define debug_prefix()                          __debug_prefix(DEBUG_OUTPUT)

#define debug_println_notempty(msg)             if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE && msg.length()) { debug_prefix();  DEBUG_OUTPUT.println(msg); }
#define debug_print(msg)                        if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { DEBUG_OUTPUT.print(msg); }
#define debug_println(msg)                      if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { debug_prefix(); DEBUG_OUTPUT.println(msg); }
#define debug_printf(fmt, ...)                  if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { debug_prefix(); DEBUG_OUTPUT.printf(fmt, ## __VA_ARGS__); }
#define debug_printf_P(fmt, ...)                if (DebugHelper::__state == DEBUG_HELPER_STATE_ACTIVE) { debug_prefix(); DEBUG_OUTPUT.printf_P(fmt, ## __VA_ARGS__); }

#define debug_print_result(result)              result
#define __debug_print_result(result)              DebugHelperPosition(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__).printResult(result)

// templkate <class T>
// T debug_print_result(T )

#define IF_DEBUG(...)                           __VA_ARGS__

#if DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_SOURCE_ARGS                       const char *_debug_filename, int _debug_lineno, const char *_debug_function,
#define DEBUG_SOURCE_ADD_ARGS                   __BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__ ,
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

#else

#define DEBUG_HELPER_INIT()         ;
#define DEBUG_HELPER_SILENT()       ;

#define __debugbreak_and_panic()                        panic();
#define __debugbreak_and_panic_printf_P(fmt, ...)       Serial.printf_P(fmt, ## __VA_ARGS__); panic();

#define debug_println_notempty(msg) ;
#define debug_print(...)            ;
#define debug_println(...)          ;
#define debug_printf(...)           ;
#define debug_printf_P(...)         ;
#define debug_wifi_diag()           ;
#define debug_prefix(...)           ;

#define debug_print_result(result)              result

#define debug_helper_set_src()          ;

#define IF_DEBUG(...)

#define DEBUG_SOURCE_ARGS
#define DEBUG_SOURCE_ADD_ARGS
#define DEBUG_SOURCE_PASS_ARGS
#define DEBUG_SOURCE_APPEND_ARGS
#define DEBUG_SOURCE_FORMAT

#endif

#include "debug_helper_enable.h"
