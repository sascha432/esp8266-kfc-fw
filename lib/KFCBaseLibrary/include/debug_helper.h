/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <Print.h>
#include <StreamWrapper.h>
#include <vector>
#include "constexpr_tools.h"
#include "section_defines.h"


#if _MSC_VER
#ifndef DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_INCLUDE_SOURCE_INFO                           1
#endif
extern unsigned long millis(void);
#define __DEBUG_FUNCTION__                                  DebugContext::pretty_function(__FUNCSIG__)
#else
#define __DEBUG_FUNCTION__                                  __FUNCTION__
#endif

#ifndef DEBUG_VT100_SUPPORT
#define DEBUG_VT100_SUPPORT                                 1
#endif

#if 1
#define __BASENAME_FILE__                                   StringConstExpr::basename(__FILE__)
#else
#define __BASENAME_FILE__                                   __FILE__
#endif

// invokes panic() on ESP8266/32, calls __debugbreak() with visual studio to intercept and debug the error
extern int ___debugbreak_and_panic(const char *filename, int line, const char *function);

#define __debugbreak_and_panic()                            ___debugbreak_and_panic(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)

#if DEBUG

extern const char ___debugPrefix[] PROGMEM;

enum class ValidatePointerType {
    P_HEAP = 0x01,
    P_STACK = 0x02,
    P_PROGMEM = 0x04,
    P_ALIGNED = 0x08,
    P_NULL = 0x10,
    P_NOSTRING = 0x20,
    P_HS = P_HEAP|P_STACK,
    P_HSU = P_HEAP|P_STACK|P_NOSTRING,
    P_HP = P_HEAP|P_PROGMEM,
    P_HPS = P_HEAP|P_STACK|P_PROGMEM,
    P_NPS = P_STACK|P_PROGMEM,
    P_NHS = P_HEAP|P_STACK|P_NULL,
    P_NHP = P_HEAP|P_PROGMEM|P_NULL,
    P_NHPS = P_HEAP|P_STACK|P_PROGMEM|P_NULL,
    P_ANPS = P_STACK|P_PROGMEM|P_NULL|P_ALIGNED,
    P_AHS = P_HEAP|P_STACK|P_ALIGNED,
    P_AHP = P_HEAP|P_PROGMEM|P_ALIGNED,
    P_AHPS = P_HEAP|P_STACK|P_PROGMEM|P_ALIGNED,
    P_APS = P_STACK|P_PROGMEM|P_ALIGNED,
    P_ANHS = P_HEAP|P_STACK|P_NULL|P_ALIGNED,
    P_ANHP = P_HEAP|P_PROGMEM|P_NULL|P_ALIGNED,
    P_ANHPS = P_HEAP|P_STACK|P_PROGMEM|P_NULL|P_ALIGNED,
};

void *__validatePointer(const void *ptr, ValidatePointerType type, const char *file, int line, const char *func);
String &__validatePointer(const String &str, ValidatePointerType type, const char *file, int line, const char *func);

template<typename _Ta>
inline _Ta *__validatePointer(const _Ta *ptr, ValidatePointerType type, const char *file, int line, const char *func) {
    return reinterpret_cast<_Ta *>(__validatePointer(reinterpret_cast<const void *>(ptr), type, file, line, func));
}

#define VP_HS                            ValidatePointerType::P_HS
#define VP_HSU                           ValidatePointerType::P_HSU
#define VP_HP                            ValidatePointerType::P_HP
#define VP_HPS                           ValidatePointerType::P_HPS
#define VP_PS                            ValidatePointerType::P_PS
#define VP_NHS                           ValidatePointerType::P_NHS
#define VP_NHP                           ValidatePointerType::P_NHP
#define VP_NHPS                          ValidatePointerType::P_NHPS
#define VP_NPS                           ValidatePointerType::P_NPS
#define __DBG_validatePointer(ptr, type) __validatePointer(ptr, type, __BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)

// call in setup, after initializing the output stream
#define DEBUG_HELPER_INIT()                                 DebugContext::__state = DEBUG_HELPER_STATE_DEFAULT;
#define DEBUG_HELPER_SILENT()                               DebugContext::__state = DEBUG_HELPER_STATE_DISABLED;
#define DEBUG_HELPER_PUSH_STATE()                           auto __debug_helper_state = DebugContext::__state;
#define DEBUG_HELPER_POP_STATE()                            DebugContext::__state = __debug_helper_state;

#ifndef DEBUG_HELPER_STATE_DEFAULT
#define DEBUG_HELPER_STATE_DEFAULT                          DEBUG_HELPER_STATE_ACTIVE
#endif

#define DEBUG_HELPER_STATE_DISABLED                         0
#define DEBUG_HELPER_STATE_ACTIVE                           1

#if DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_HELPER_POSITION                               __BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__
#else
#define DEBUG_HELPER_POSITION
#endif

#if DEBUG_VT100_SUPPORT

#define __vt100_code_reset                                  "m"
#define __vt100_code_bold                                   "1;"
#define __vt100_code_yellow                                 "33m"
#define __vt100_code_bold_red                               "1;31m"
#define __vt100_code_bold_green                             "1;32m"
#define __vt100_code_bold_cyan                              "1;36m"

#define _VT100_STR(name)                                    __vt100_code_##name
#define _VT100(name)                                        "\033[" _VT100_STR(name)

#else

#define _VT100(...)                                         ""

#endif

#define __DBG_newline                                       "\n"


// regular debug functions
#define __DBG_print(arg)                                    debug_print(F(arg __DBG_newline))
#define __DBG_printf(fmt, ...)                              debug_printf(PSTR(fmt __DBG_newline), ##__VA_ARGS__)
#define __DBG_printf_E(fmt, ...)                            debug_printf(PSTR(_VT100(bold_red) fmt _VT100(reset) __DBG_newline), ##__VA_ARGS__)
#define __DBG_printf_W(fmt, ...)                            debug_printf(PSTR(_VT100(yellow) fmt _VT100(reset) __DBG_newline), ##__VA_ARGS__)
#define __DBG_printf_N(fmt, ...)                            debug_printf(PSTR(_VT100(bold_green) fmt _VT100(reset) __DBG_newline), ##__VA_ARGS__)
#define __DBG_println()                                     debug_print(F(__DBG_newline))
#define __DBG_panic(fmt, ...)                               (DEBUG_OUTPUT.printf_P(PSTR(fmt __DBG_newline), ## __VA_ARGS__) && __debugbreak_and_panic())
#define __DBG_assert(cond)                                  (!(cond) ? (__DBG_print(_VT100(bold_red) "assert( " _STRINGIFY(cond) ") FAILED" _VT100(reset)) && DebugContext::reportAssert(DebugContext_ctor(), F(_STRINGIFY(cond)))) : false)
#define __DBG_assert_printf(cond, fmt, ...)                 (!(cond) ? (__DBG_print(_VT100(bold_red) "assert( " _STRINGIFY(cond) ") FAILED" _VT100(reset)) && __DBG_printf(fmt, ##__VA_ARGS__) && DebugContext::reportAssert(DebugContext_ctor(), F(_STRINGIFY(cond)))) : false)
#define __DBG_assert_panic(cond, fmt, ...)                  (__DBG_assert(cond) ? __DBG_panic(fmt, ##__VA_ARGS__) : false)
#define __DBG_print_result(result)                          debug_print_result(result)
#define __DBG_printf_result(result, fmt, ...)               debug_printf_result(result, PSTR(fmt __DBG_newline), ##__VA_ARGS__)
#define __DBG_prints_result(result, to_string)              debug_prints_result(result, to_string)
#define __DBG_IF(...)                                       __VA_ARGS__
#define __DBG_N_IF(...)
#define __DBG_S_IF(a, b)                                    __DBG_IF(a)__DBG_N_IF(b)

// MSVC version

#ifndef _ASSERTE
#define _ASSERTE(cond)                                      __DBG_assert(cond)
#endif
#ifndef _ASSERT_EXPR
#define _ASSERT_EXPR(cond, fmt, ...)                        __DBG_assert_printf(cond, fmt, ##__VA_ARGS__)
#endif

// memory management

#include "DebugContext.h"

// validate pointers from alloc
#if _MSC_VER

#define ___IsValidStackPointer(ptr)                         _CrtIsValidHeapPointer(ptr)
#define ___IsValidHeapPointer(ptr)                          _CrtIsValidHeapPointer(ptr)
#define ___IsValidDRAMPointer(ptr)                          _CrtIsValidHeapPointer(ptr)
#define ___IsValidPROGMEMPointer(ptr)                       _CrtIsValidHeapPointer(ptr)
#define ___IsValidPointer(ptr)                              _CrtIsValidHeapPointer(ptr)
#define ___isValidPointerAlignment(ptr)                     true

#elif defined(ESP8266)

#define ___IsValidStackPointer(ptr)                         ((uint32_t)ptr >= SECTION_HEAP_END_ADDRESS && (uint32_t)ptr <= SECTION_STACK_END_ADDRESS)
#define ___IsValidHeapPointer(ptr)                          ((uint32_t)ptr >= SECTION_HEAP_START_ADDRESS && (uint32_t)ptr < SECTION_DRAM_END_ADDRESS)
#define ___IsValidDRAMPointer(ptr)                          ((uint32_t)ptr >= SECTION_DRAM_START_ADDRESS && (uint32_t)ptr < SECTION_HEAP_END_ADDRESS)
#define ___IsValidPROGMEMPointer(ptr)                       ((uint32_t)ptr >= SECTION_IROM0_TEXT_START_ADDRESS && (uint32_t)ptr < SECTION_IROM0_TEXT_END_ADDRESS)
#define ___IsValidPointer(ptr)                              (___IsValidHeapPointer(ptr) || ___IsValidPROGMEMPointer(ptr))
#define ___isValidPointerAlignment(ptr)                     (((uint32_t)ptr & 0x03) == 0)

#else

#define ___IsValidStackPointer(ptr)                         true
#define ___IsValidHeapPointer(ptr)                          true
#define ___IsValidDRAMPointer(ptr)                          true
#define ___IsValidPROGMEMPointer(ptr)                       true
#define ___IsValidPointer(ptr)                              true
#define ___isValidPointerAlignment(ptr)                     true

#endif

#define __DBG_check_alloc(ptr, allow_null)                  __DBG_assert_printf((allow_null && ptr == nullptr) || (!allow_null && ptr != nullptr && ___IsValidHeapPointer(ptr)), "alloc=%08x", ptr)
#define __DBG_check_alloc_no_null(ptr)                      __DBG_check_alloc(ptr, false)
#define __DBG_check_alloc_null(ptr)                         __DBG_check_alloc(ptr, true)
#define __DBG_check_alloc_aligned(ptr)                      __DBG_assert_printf((ptr != nullptr) && ___isValidPointerAlignment(ptr), "not aligned=%08x", ptr)

// validate pointers to RAM or PROGMEM

#define __DBG_check_ptr_no_null(ptr)                        __DBG_assert_printf((ptr != nullptr) && ___IsValidPointer(ptr), "pointer=%08x", ptr)
#define __DBG_check_ptr_null(ptr)                           __DBG_assert_printf((ptr == nullptr) || ___IsValidPointer(ptr), "pointer=%08x", ptr)
#define __DBG_check_ptr(ptr, allow_null)                    __DBG_assert_printf((allow_null && ptr == nullptr) || (!allow_null && ptr != nullptr && ___IsValidPointer(ptr)), "pointer=%08x", ptr)

#define __DBG_function()                                    DebugContext::Guard guard(DebugContext_ctor());
#define __DBG_function_printf(fmt, ...)                     DebugContext::Guard guard(DebugContext_ctor(), PSTR(fmt), ##__VA_ARGS__);

// local functions that need to be activated by including debug_helper_[enable|disable].h
#define __LDBG_IF(...)
#define __LDBG_N_IF(...)                                    __VA_ARGS__
#define __LDBG_S_IF(a, b)                                   __LDBG_IF(a)__LDBG_N_IF(b)
#define __LDBG_panic(...)                                   __LDBG_IF(__DBG_panic(__VA_ARGS__))
#define __LDBG_printf(...)                                  __LDBG_IF(__DBG_printf(__VA_ARGS__))
#define __LDBG_printf_E(...)                                __LDBG_IF(__DBG_printf_E(__VA_ARGS__))
#define __LDBG_printf_W(...)                                __LDBG_IF(__DBG_printf_W(__VA_ARGS__))
#define __LDBG_printf_N(...)                                __LDBG_IF(__DBG_printf_N(__VA_ARGS__))
#define __LDBG_print(...)                                   __LDBG_IF(__DBG_print(__VA_ARGS__))
#define __LDBG_println(...)                                 __LDBG_IF(__DBG_println(__VA_ARGS__))
#define __LDBG_assert(...)                                  __DBG_assert(__VA_ARGS__)
#define __LDBG_assert_printf(cond, fmt, ...)                __DBG_assert_printf(cond, fmt, ##__VA_ARGS__)
#define __LDBG_assert_panic(cond, fmt, ...)                 __DBG_assert_panic(cond, fmt, ##__VA_ARGS__)
#define __LDBG_print_result(result, ...)                    __LDBG_S_IF(__DBG_print_result(result, ##__VA_ARGS__), result)
#define __LDBG_function()                                   __LDBG_IF(__DBG_function())
#define __LDBG_function_printf(fmt, ...)                    __LDBG_IF(__DBG_function_printf(fmt, ##__VA_ARGS__))

#define __LDBG_check_alloc(...)                             __LDBG_IF(__DBG_check_alloc(__VA_ARGS__))
#define __LDBG_check_alloc_no_null(...)                     __LDBG_IF(__DBG_check_alloc_no_null(__VA_ARGS__))
#define __LDBG_check_alloc_null(...)                        __LDBG_IF(__DBG_check_alloc_null(__VA_ARGS__))
#define __LDBG_check_alloc_aligned(...)                     __LDBG_IF(__DBG_check_alloc_aligned(__VA_ARGS__))
#define __LDBG_check_ptr(...)                               __LDBG_IF(__DBG_check_ptr(__VA_ARGS__))
#define __LDBG_check_ptr_no_null(...)                       __LDBG_IF(__DBG_check_ptr_no_null(__VA_ARGS__))
#define __LDBG_check_ptr_null(...)                          __LDBG_IF(__DBG_check_ptr_null(__VA_ARGS__))

// depricated functions below

#if DEBUG_INCLUDE_SOURCE_INFO
#   if ESP32 && CONFIG_HEAP_POISONING_COMPREHENSIVE
#       define                                              __debug_prefix(stream) (stream.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), can_yield(), __DEBUG_FUNCTION__) + heap_caps_check_integrity_all(true))
#   else
#       define                                              __debug_prefix(stream) stream.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), can_yield(), __DEBUG_FUNCTION__)
#   endif
#else
#   define                                                  __debug_prefix(stream) stream.print(FPSTR(___debugPrefix))
#endif
#define debug_prefix()                                      __debug_prefix(DEBUG_OUTPUT)


static inline int DEBUG_OUTPUT_flush() {
    DEBUG_OUTPUT.flush();
    return 1;
}

#define isDebugContextActive()                              DebugContext::isActive(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)
#define DebugContext_ctor()                                 DebugContext(DEBUG_HELPER_POSITION)
#define DebugContext_prefix(...)                            { if (isDebugContextActive()) { debug_prefix(); __VA_ARGS__; } }

// debug_print* macros return 0 if debugging is disabled or >=1, basically the length of the data that was sent + 1
#define debug_print(msg)                                    (isDebugContextActive() ? (debug_prefix() + DEBUG_OUTPUT.print(msg) + DEBUG_OUTPUT_flush()) : 0)
#define debug_println(...)                                  (isDebugContextActive() ? (debug_prefix() + DEBUG_OUTPUT.println(__VA_ARGS__) + DEBUG_OUTPUT_flush()) : 0)
#define debug_printf(fmt, ...)                              (isDebugContextActive() ? (debug_prefix() + DEBUG_OUTPUT.printf(fmt, ##__VA_ARGS__) + DEBUG_OUTPUT_flush()) : 0)
#define debug_printf_P(fmt, ...)                            (isDebugContextActive() ? (debug_prefix() + DEBUG_OUTPUT.printf_P(fmt, ##__VA_ARGS__) + DEBUG_OUTPUT_flush()) : 0)

// Print::println(result) must be possible
#define debug_print_result(result)                          DebugContext_ctor().printResult(result)
// to_string needs to convert result to a value that can be passed to (const String &) ...
#define debug_prints_result(result, to_string)              DebugContext_ctor().printsResult(result, to_string(result))
#define debug_printf_result(result, fmt, ...)               DebugContext_ctor().printfResult(result, fmt, #__VA_ARGS__)

#define __DBG_store_position()                              DebugContext::__store_pos(std::move(DebugContext_ctor()))

#define __SDBG_printf(fmt, ...)                             ::printf(PSTR(fmt "\n"), ##__VA_ARGS__);

#define IF_DEBUG(...)                                       __DBG_IF(__VA_ARGS__)
#define _IF_DEBUG(...)                                      __LDBG_IF(__VA_ARGS__)

#define _debug_print(...)                                   _IF_DEBUG(debug_print(__VA_ARGS__))
#define _debug_println(...)                                 _IF_DEBUG(debug_println(__VA_ARGS__))
#define _debug_printf(...)                                  _IF_DEBUG(debug_printf(__VA_ARGS__))
#define _debug_printf_P(...)                                _IF_DEBUG(debug_printf_P(__VA_ARGS__))
#define _debug_print_result(result)                         _IF_DEBUG(debug_print_result(result))
#define __SLDBG_printf(...)                                 _IF_DEBUG(__SDBG_printf(__VA_ARGS__))
#define __SLDBG_panic(...)                                  _IF_DEBUG(__SDBG_printf(__VA_ARGS__); panic();)

#if DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_SOURCE_ARGS                                   const char *_debug_filename, int _debug_lineno, const char *_debug_function,
#define DEBUG_SOURCE_ADD_ARGS                               __BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__,
#define DEBUG_SOURCE_PASS_ARGS                              _debug_filename, _debug_lineno, _debug_function,
#define DEBUG_SOURCE_APPEND_ARGS                            , _debug_filename, _debug_lineno, _debug_function
#define DEBUG_SOURCE_FORMAT                                 " (%s:%u@%s)"
#else
#define DEBUG_SOURCE_ARGS
#define DEBUG_SOURCE_ADD_ARGS
#define DEBUG_SOURCE_PASS_ARGS
#define DEBUG_SOURCE_APPEND_ARGS
#define DEBUG_SOURCE_FORMAT
#endif

#else

// deprecated

#define DEBUG_HELPER_INIT()                                 ;
#define DEBUG_HELPER_SILENT()                               ;
#define debug_print(...)                                    ;
#define debug_println(...)                                  ;
#define debug_printf(...)                                   ;
#define debug_printf_P(...)                                 ;
#define debug_wifi_diag()                                   ;
#define debug_prefix(...)                                   ;
#define debug_print_result(result)                          result
#define IF_DEBUG(...)
#define _IF_DEBUG(...)
#define _debug_print_result(result)                         result
#define DEBUG_SOURCE_ARGS
#define DEBUG_SOURCE_ADD_ARGS
#define DEBUG_SOURCE_PASS_ARGS
#define DEBUG_SOURCE_APPEND_ARGS
#define DEBUG_SOURCE_FORMAT

// MSVC version

#ifndef _ASSERTE
#define _ASSERTE(...)
#endif
#ifndef _ASSERT_EXPR
#define _ASSERT_EXPR(...)
#endif

// new functions

#define __DBG_validatePointer(ptr, ...)                     ptr

#define isDebugContextActive()                              false
#define DebugContext_prefix(...)

#define __DBG_print(...)                                    ;
#define __DBG_printf(...)                                   ;
#define __DBG_println()                                     ;
#define __DBG_panic(...)                                    ;
#define __DBG_assert(...)                                   ;
#define __DBG_assert_printf(...)                            ;
#define __DBG_assert_panic(...)                             ;
#define __DBG_print_result(result, ...)                     result
#define __DBG_IF(...)

#define __LDBG_IF(...)
#define __LDBG_N_IF(...)                                    __VA_ARGS__
#define __LDBG_S_IF(a, b)                                   b
#define __LDBG_panic(...)                                   ;
#define __LDBG_printf(...)                                  ;
#define __LDBG_print(...)                                   ;
#define __LDBG_println(...)                                 ;
#define __LDBG_assert(...)                                  ;
#define __LDBG_assert_printf(...)                           ;
#define __LDBG_assert_panic(...)                            ;
#define __LDBG_print_result(result, ...)                    result

#define __LDBG_check_alloc(...)                             ;
#define __LDBG_check_alloc_no_null(...)                     ;
#define __LDBG_check_alloc_null(...)                        ;
#define __LDBG_check_alloc_aligned(...)                     ;
#define __LDBG_check_ptr(...)                               ;
#define __LDBG_check_ptr_no_null(...)                       ;
#define __LDBG_check_ptr_null(...)                          ;

#endif

#include "debug_helper_enable.h"
