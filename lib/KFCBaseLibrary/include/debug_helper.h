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

#ifdef _MSC_VER
#ifndef DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_INCLUDE_SOURCE_INFO                           1
#endif
extern unsigned long millis(void);
#define ____DEBUG_FUNCTION__                                __FUNCSIG__
#else
#define ____DEBUG_FUNCTION__                                __FUNCTION__
#endif

#if 1
#define __BASENAME_FILE__                                   StringConstExpr::basename(__FILE__)
#else
#define __BASENAME_FILE__                                   __FILE__
#endif

#if 1
#define __DEBUG_FUNCTION__                                  DebugContext::pretty_function(____DEBUG_FUNCTION__)
#else
#define __DEBUG_FUNCTION__                                  ____DEBUG_FUNCTION__
#endif

#define __NDBG_allocator                                    std::allocator
#if _MSC_VER
#define __NDBG_operator_new(...)                            std::malloc(__VA_ARGS__)
#define __NDBG_operator_delete(...)                         std::free(__VA_ARGS__)
#else
#include <new>
#define __NDBG_operator_new(...)                            ::operator new(__VA_ARGS__)
#define __NDBG_operator_delete(...)                         ::operator delete(__VA_ARGS__)
#endif
#define __NDBG_new(name, ...)                               new name(__VA_ARGS__)
#define __NDBG_delete(ptr)                                  delete ptr
#define __NDBG_delete_remove(ptr)                           ;
#define __NDBG_new_array(num, name, ...)                    new name[num](__VA_ARGS__)
#define __NDBG_delete_array(ptr)                            delete[] ptr
#define __NDBG_track_new(...)                               ;
#define __NDBG_track_delete(...)                            ;
#define __NDBG_malloc(size)                                 malloc(size)
#define __NDBG_realloc(ptr, size)                           realloc(ptr, size)
#define __NDBG_calloc(num, size)                            calloc(num, size)
#define __NDBG_free(ptr)                                    free(ptr)
#define __NDBG_malloc_str(size)                             reinterpret_cast<char *>(__NDBG_malloc(size))
#define __NDBG_malloc_buf(size)                             reinterpret_cast<uint8_t *>(__NDBG_malloc(size))
#define __NDBG_strdup(str)                                  strdup(str)
#define __NDBG_strdup_P(str)                                strdup_P(str)

extern "C" bool can_yield();

// invokes panic() on ESP8266/32, calls __debugbreak() with visual studio to intercept and debug the error
extern int ___debugbreak_and_panic(const char *filename, int line, const char *function);

#define __debugbreak_and_panic()                            ___debugbreak_and_panic(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)

#if DEBUG

extern const char ___debugPrefix[] PROGMEM;

// call in setup, after initializing the output stream
#define DEBUG_HELPER_INIT()                                 DebugContext::__state = DEBUG_HELPER_STATE_DEFAULT;
#define DEBUG_HELPER_SILENT()                               DebugContext::__state = DEBUG_HELPER_STATE_DISABLED;

#ifndef DEBUG_HELPER_STATE_DEFAULT
#define DEBUG_HELPER_STATE_DEFAULT                          DEBUG_HELPER_STATE_ACTIVE
#endif

#define DEBUG_HELPER_STATE_ACTIVE                           0
#define DEBUG_HELPER_STATE_FILTERED                         1
#define DEBUG_HELPER_STATE_DISABLED                         2

#if DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_HELPER_POSITION                               DebugContext(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)
#else
#define DEBUG_HELPER_POSITION                               DebugContext()
#endif

// regular debug functions
#define __DBG_print(arg)                                    debug_println(F(arg))
#define __DBG_printf(fmt, ...)                              debug_printf(PSTR(fmt "\n"), ##__VA_ARGS__)
#define __DBG_println()                                     debug_println()
#define __DBG_panic(fmt, ...)                               (DEBUG_OUTPUT.printf_P(PSTR(fmt "\n"), ## __VA_ARGS__) && __debugbreak_and_panic())
#define __DBG_assert(cond)                                  (!(cond) ? (__DBG_print("assert( " _STRINGIFY(cond) ")") && true) : false)
#define __DBG_assert_printf(cond, fmt, ...)                 (!(cond) ? (__DBG_print("assert( " _STRINGIFY(cond) ")") && __DBG_printf(fmt, ##__VA_ARGS__)) : false)
#define __DBG_assert_panic(cond, fmt, ...)                  (!(cond) ? (__DBG_print("assert( " _STRINGIFY(cond) ")") && __DBG_panic(fmt, ##__VA_ARGS__)) : false)
#define __DBG_print_result(result)                          debug_print_result(result)
#define __DBG_printf_result(result, fmt, ...)               debug_printf_result(result, PSTR(fmt "\n"), ##__VA_ARGS__)
#define __DBG_prints_result(result, to_string)              debug_prints_result(result, to_string)
#define __DBG_IF(...)                                       __VA_ARGS__
#define __DBG_N_IF(...)
#define __DBG_S_IF(a, b)                                    __DBG_IF(a) __DBG_N_IF(b)

// memory management

#include "DebugContext.h"

#ifndef HAVE_MEM_DEBUG
#define HAVE_MEM_DEBUG                                      0
#endif

// debug
#if HAVE_MEM_DEBUG
#include "DebugMemoryHelper.h"
#define __DBG_allocator                                     __DebugAllocator
#define __DBG_track_allocator                               __TrackAllocator
#define __DBG_operator_new(...)                             KFCMemoryDebugging::_new(DEBUG_HELPER_POSITION, (__VA_ARGS__), __NDBG_operator_new(__VA_ARGS__))
#define __DBG_operator_delete(ptr)                          __NDBG_operator_delete(KFCMemoryDebugging::_delete(DEBUG_HELPER_POSITION, ptr))
#define __DBG_track_new(size)                               KFCMemoryDebugging::_track_new(size)
#define __DBG_track_delete(size)                            KFCMemoryDebugging::_track_delete(nullptr, size)
#define __DBG_new(name, ...)                                KFCMemoryDebugging::_new(DEBUG_HELPER_POSITION, sizeof(name), new name(__VA_ARGS__))
#define __DBG_delete(ptr)                                   delete KFCMemoryDebugging::_delete(DEBUG_HELPER_POSITION, ptr)
#define __DBG_delete_remove(ptr)                            KFCMemoryDebugging::_delete(DEBUG_HELPER_POSITION, ptr)
#define __DBG_new_array(num, name, ...)                     KFCMemoryDebugging::_new(DEBUG_HELPER_POSITION, num * sizeof(name), new name[num](__VA_ARGS__))
#define __DBG_delete_array(ptr)                             delete[] KFCMemoryDebugging::_delete(DEBUG_HELPER_POSITION, ptr)
#define __DBG_malloc(size)                                  KFCMemoryDebugging::_alloc(DEBUG_HELPER_POSITION, size, malloc(size))
#define __DBG_realloc(ptr, size)                            KFCMemoryDebugging::_realloc(DEBUG_HELPER_POSITION, size, ptr, realloc(ptr, size))
#define __DBG_calloc(num, size)                             KFCMemoryDebugging::_alloc(DEBUG_HELPER_POSITION, num * size, calloc(num, size))
#define __DBG_free(ptr)                                     free(KFCMemoryDebugging::_free(DEBUG_HELPER_POSITION, ptr))
#define __DBG_malloc_str(size)                              reinterpret_cast<char *>(__DBG_malloc(size))
#define __DBG_malloc_buf(size)                              reinterpret_cast<uint8_t *>(__DBG_malloc(size))
#define __DBG_strdup(str)                                   KFCMemoryDebugging::_alloc(DEBUG_HELPER_POSITION, strdup(str))
#define __DBG_strdup_P(str)                                 KFCMemoryDebugging::_alloc(DEBUG_HELPER_POSITION, strdup_P(str))
#else
#define __DBG_allocator                                     __NDBG_allocator
#define __DBG_operator_new(...)                             __NDBG_operator_new(__VA_ARGS__)
#define __DBG_operator_delete(...)                          __NDBG_operator_delete(__VA_ARGS__)
#define __DBG_track_new(...)                                __NDBG_track_new(__VA_ARGS__)
#define __DBG_track_delete(...)                             __NDBG_track_delete(__VA_ARGS__)
#define __DBG_new(...)                                      __NDBG_new(__VA_ARGS__)
#define __DBG_delete(...)                                   __NDBG_delete(__VA_ARGS__)
#define __DBG_delete_remove(...)                            __NDBG_delete_remove(__VA_ARGS__)
#define __DBG_new_array(...)                                __NDBG_new_array(__VA_ARGS__)
#define __DBG_delete_array(...)                             __NDBG_delete_array(__VA_ARGS__)
#define __DBG_malloc(...)                                   __NDBG_malloc(__VA_ARGS__)
#define __DBG_realloc(...)                                  __NDBG_realloc(__VA_ARGS__)
#define __DBG_calloc(...)                                   __NDBG_calloc(__VA_ARGS__)
#define __DBG_free(...)                                     __NDBG_free(__VA_ARGS__)
#define __DBG_malloc_str(...)                               __NDBG_malloc_str(__VA_ARGS__)
#define __DBG_malloc_buf(...)                               __NDBG_malloc_buf(__VA_ARGS__)
#define __DBG_strdup(...)                                   __NDBG_strdup(__VA_ARGS__)
#define __DBG_strdup_P(...)                                 __NDBG_strdup_P(__VA_ARGS__)
#endif

// debug if local and memory debugging is enabled
#define __LDBG_allocator                                    __LMDBG_S_IF(__DBG_allocator, __NDBG_allocator)
#define __LDBG_track_allocator                              __LMDBG_S_IF(__DBG_track_allocator, __NDBG_allocator)
#define __LDBG_operator_new(...)                            __LMDBG_S_IF(__DBG_operator_new(__VA_ARGS__), __NDBG_operator_new(__VA_ARGS__))
#define __LDBG_operator_delete(...)                         __LMDBG_S_IF(__DBG_operator_delete(__VA_ARGS__), __NDBG_operator_delete(__VA_ARGS__))
#define __LDBG_track_new(...)                               __LMDBG_S_IF(__DBG_track_new(__VA_ARGS__), __NDBG_track_new(__VA_ARGS__))
#define __LDBG_track_delete(...)                            __LMDBG_S_IF(__DBG_track_delete(__VA_ARGS__), __NDBG_track_delete(__VA_ARGS__))
#define __LDBG_new(...)                                     __LMDBG_S_IF(__DBG_new(__VA_ARGS__), __NDBG_new(__VA_ARGS__))
#define __LDBG_delete(...)                                  __LMDBG_S_IF(__DBG_delete(__VA_ARGS__), __NDBG_delete(__VA_ARGS__))
#define __LDBG_delete_remove(...)                           __LMDBG_S_IF(__DBG_delete_remove(__VA_ARGS__), __NDBG_delete_remove(__VA_ARGS__))
#define __LDBG_new_array(...)                               __LMDBG_S_IF(__DBG_new_array(__VA_ARGS__), __NDBG_new_array(__VA_ARGS__))
#define __LDBG_delete_array(...)                            __LMDBG_S_IF(__DBG_delete_array(__VA_ARGS__), __NDBG_delete_array(__VA_ARGS__))
#define __LDBG_malloc(...)                                  __LMDBG_S_IF(__DBG_malloc(__VA_ARGS__), __NDBG_malloc(__VA_ARGS__))
#define __LDBG_realloc(...)                                 __LMDBG_S_IF(__DBG_realloc(__VA_ARGS__), __NDBG_realloc(__VA_ARGS__))
#define __LDBG_calloc(...)                                  __LMDBG_S_IF(__DBG_calloc(__VA_ARGS__), __NDBG_calloc(__VA_ARGS__))
#define __LDBG_free(...)                                    __LMDBG_S_IF(__DBG_free(__VA_ARGS__), __NDBG_free(__VA_ARGS__))
#define __LDBG_malloc_str(...)                              __LMDBG_S_IF(__DBG_malloc_str(__VA_ARGS__), __NDBG_malloc_str(__VA_ARGS__))
#define __LDBG_malloc_buf(...)                              __LMDBG_S_IF(__DBG_malloc_buf(__VA_ARGS__), __NDBG_malloc_buf(__VA_ARGS__))
#define __LDBG_strdup(...)                                  __LMDBG_S_IF(__DBG_strdup(__VA_ARGS__), __NDBG_strdup(__VA_ARGS__))
#define __LDBG_strdup_P(...)                                __LMDBG_S_IF(__DBG_strdup_P(__VA_ARGS__), __NDBG_strdup_P(__VA_ARGS__))

// validate pointers from alloc

#if _MSC_VER

#define ___IsValidHeapPointer(ptr)                          _CrtIsValidHeapPointer(ptr)
#define ___IsValidPROGMEMPointer(ptr)                       _CrtIsValidHeapPointer(ptr)
#define ___IsValidPointer(ptr)                              _CrtIsValidHeapPointer(ptr)
#define ___isValidPointerAlignment(ptr)                     true

#elif defined(ESP8266)

#define ___IsValidHeapPointer(ptr)                          ((uint32_t)ptr >= SECTION_HEAP_START_ADDRESS && (uint32_t)ptr < SECTION_HEAP_END_ADDRESS)
#define ___IsValidPROGMEMPointer(ptr)                       ((uint32_t)ptr >= SECTION_IROM0_TEXT_START_ADDRESS && (uint32_t)ptr < SECTION_IROM0_TEXT_END_ADDRESS)
#define ___IsValidPointer(ptr)                              (___IsValidHeapPointer(ptr) || ___IsValidPROGMEMPointer(ptr))
#define ___isValidPointerAlignment(ptr)                     (((uint32_t)ptr & 0x03) == 0)

#else

#error missing

#endif

#define __DBG_check_alloc(ptr, allow_null)                  __DBG_assert_printf((allow_null && ptr == nullptr) || (!allow_null && ptr != nullptr && ___IsValidHeapPointer(ptr)), "alloc=%08x", ptr)
#define __DBG_check_alloc_no_null(ptr)                      __DBG_check_alloc(ptr, false)
#define __DBG_check_alloc_null(ptr)                         __DBG_check_alloc(ptr, true)
#define __DBG_check_alloc_aligned(ptr)                      __DBG_assert_printf((ptr != nullptr) && ___isValidPointerAlignment(ptr), "not aligned=%08x", ptr)

// validate pointers to RAM or PROGMEM

#define __DBG_check_ptr_no_null(ptr)                        __DBG_assert_printf((ptr != nullptr) && ___IsValidPointer(ptr), "pointer=%08x", ptr)
#define __DBG_check_ptr_null(ptr)                           __DBG_assert_printf((ptr == nullptr) || ___IsValidPointer(ptr), "pointer=%08x", ptr)
#define __DBG_check_ptr(ptr, allow_null)                    __DBG_assert_printf((allow_null && ptr == nullptr) || (!allow_null && ptr != nullptr && ___IsValidPointer(ptr)), "pointer=%08x", ptr)

// local functions that need to be activated by including debug_helper_[enable|disable].h
#define __LDBG_IF(...)
#define __LDBG_N_IF(...)                                    __VA_ARGS__
#if HAVE_MEM_DEBUG
#define __LMDBG_IF(...)
#define __LMDBG_N_IF(...)                                   __VA_ARGS__
#else
#define __LMDBG_IF(...)                                     __VA_ARGS__
#define __LMDBG_N_IF(...)
#endif
#define __LDBG_S_IF(a, b)                                   __LDBG_IF(a) __LDBG_N_IF(b)
#define __LMDBG_S_IF(a, b)                                  __LMDBG_IF(a) __LMDBG_N_IF(b)
#define __LDBG_panic(...)                                   __LDBG_IF(__DBG_panic(__VA_ARGS__))
#define __LDBG_printf(...)                                  __LDBG_IF(__DBG_printf(__VA_ARGS__))
#define __LDBG_print(...)                                   __LDBG_IF(__DBG_print(__VA_ARGS__))
#define __LDBG_println(...)                                 __LDBG_IF(__DBG_println(__VA_ARGS__))
#define __LDBG_assert(...)                                  __LDBG_IF(__DBG_assert(__VA_ARGS__))
#define __LDBG_assert_printf(...)                           __LDBG_IF(__DBG_assert_printf(__VA_ARGS__))
#define __LDBG_assert_panic(...)                            __LDBG_IF(__DBG_assert_panic(__VA_ARGS__))
#define __LDBG_print_result(result, ...)                    __LDBG_S_IF(__DBG_print_result(result, #__VA_ARGS__), result)

#define __LDBG_check_alloc(...)                             __LDBG_IF(__DBG_check_alloc(__VA_ARGS__))
#define __LDBG_check_alloc_no_null(...)                     __LDBG_IF(__DBG_check_alloc_no_null(__VA_ARGS__))
#define __LDBG_check_alloc_null(...)                        __LDBG_IF(__DBG_check_alloc_null(__VA_ARGS__))
#define __LDBG_check_alloc_aligned(...)                     __LDBG_IF(__DBG_check_alloc_aligned(__VA_ARGS__))
#define __LDBG_check_ptr(...)                               __LDBG_IF(__DBG_check_ptr(__VA_ARGS__))
#define __LDBG_check_ptr_no_null(...)                       __LDBG_IF(__DBG_check_ptr_no_null(__VA_ARGS__))
#define __LDBG_check_ptr_null(...)                          __LDBG_IF(__DBG_check_ptr_null(__VA_ARGS__))

// depricated functions below

#if DEBUG_INCLUDE_SOURCE_INFO
#   define                                                  __debug_prefix(stream) stream.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), can_yield(), __DEBUG_FUNCTION__)
#else
#   define                                                  __debug_prefix(stream) stream.print(FPSTR(___debugPrefix))
#endif
#define debug_prefix()                                      __debug_prefix(DEBUG_OUTPUT)


static inline int DEBUG_OUTPUT_flush() {
    DEBUG_OUTPUT.flush();
    return 1;
}

// debug_print* macros return 0 if debugging is disabled or >=1, basically the length of the data that was sent + 1
#define debug_print(msg)                                    ((DebugContext::__state == DEBUG_HELPER_STATE_ACTIVE) ? (DEBUG_OUTPUT.print(msg) + DEBUG_OUTPUT_flush()) : 0)
#define debug_println(...)                                  ((DebugContext::__state == DEBUG_HELPER_STATE_ACTIVE) ? (debug_prefix() + DEBUG_OUTPUT.println(__VA_ARGS__) + DEBUG_OUTPUT_flush()) : 0)
#define debug_printf(fmt, ...)                              ((DebugContext::__state == DEBUG_HELPER_STATE_ACTIVE) ? (debug_prefix() + DEBUG_OUTPUT.printf(fmt, ##__VA_ARGS__) + DEBUG_OUTPUT_flush()) : 0)
#define debug_printf_P(fmt, ...)                            ((DebugContext::__state == DEBUG_HELPER_STATE_ACTIVE) ? (debug_prefix() + DEBUG_OUTPUT.printf_P(fmt, ##__VA_ARGS__) + DEBUG_OUTPUT_flush()) : 0)

// Print::println(result) must be possible
#define debug_print_result(result)                          DebugContext(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__).printResult(result)
// to_string needs to convert result to a value that can be passed to (const String &) ...
#define debug_prints_result(result, to_string)              DebugContext(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__).printsResult(result, to_string(result))
#define debug_printf_result(result, fmt, ...)               DebugContext(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__).printfResult(result, fmt, #__VA_ARGS__)

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

#include "DebugAllocator.h"

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

// new functions

#define __DBG_print(...)                                    ;
#define __DBG_printf(...)                                   ;
#define __DBG_println()                                     ;
#define __DBG_panic(...)                                    ;
#define __DBG_assert(...)                                   ;
#define __DBG_assert_printf(...)                            ;
#define __DBG_assert_panic(...)                             ;
#define __DBG_print_result(result, ...)                     result
#define __DBG_IF(...)

// memory management

#define __DBG_allocator                                     __NDBG_allocator
#define __DBG_operator_new(...)                             __NDBG_operator_new(__VA_ARGS__)
#define __DBG_operator_delete(...)                          __NDBG_operator_delete(__VA_ARGS__)
#define __DBG_new(...)                                      __NDBG_new(__VA_ARGS__)
#define __DBG_delete(...)                                   __NDBG_delete(__VA_ARGS__)
#define __DBG_delete_remove(...)                            __NDBG_delete_remove(__VA_ARGS__)
#define __DBG_new_array(...)                                __NDBG_new_array(__VA_ARGS__)
#define __DBG_delete_array(...)                             __NDBG_delete_array(__VA_ARGS__)
#define __DBG_malloc(...)                                   __NDBG_malloc(__VA_ARGS__)
#define __DBG_realloc(...)                                  __NDBG_realloc(__VA_ARGS__)
#define __DBG_calloc(...)                                   __NDBG_calloc(__VA_ARGS__)
#define __DBG_free(...)                                     __NDBG_free(__VA_ARGS__)
#define __DBG_malloc_str(...)                               __NDBG_malloc_str(__VA_ARGS__)
#define __DBG_malloc_buf(...)                               __NDBG_malloc_buf(__VA_ARGS__)
#define __DBG_strdup(...)                                   __NDBG_strdup(__VA_ARGS__)
#define __DBG_strdup_P(...)                                 __NDBG_strdup_P(__VA_ARGS__)

#define __LDBG_allocator                                    __NDBG_allocator
#define __LDBG_operator_new(...)                            __NDBG_operator_new(__VA_ARGS__)
#define __LDBG_operator_delete(...)                         __NDBG_operator_delete(__VA_ARGS__)
#define __LDBG_new(...)                                     __NDBG_new(__VA_ARGS__)
#define __LDBG_delete(...)                                  __NDBG_delete(__VA_ARGS__)
#define __LDBG_delete_remove(...)                           __NDBG_delete_remove(__VA_ARGS__)
#define __LDBG_new_array(...)                               __NDBG_new_array(__VA_ARGS__)
#define __LDBG_delete_array(...)                            __NDBG_delete_array(__VA_ARGS__)
#define __LDBG_malloc(...)                                  __NDBG_malloc(__VA_ARGS__)
#define __LDBG_realloc(...)                                 __NDBG_realloc(__VA_ARGS__)
#define __LDBG_calloc(...)                                  __NDBG_calloc(__VA_ARGS__)
#define __LDBG_free(...)                                    __NDBG_free(__VA_ARGS__)
#define __LDBG_malloc_str(...)                              __NDBG_malloc_str(__VA_ARGS__)
#define __LDBG_malloc_buf(...)                              __NDBG_malloc_buf(__VA_ARGS__)
#define __LDBG_strdup(...)                                  __NDBG_strdup(__VA_ARGS__)
#define __lDBG_strdup_P(...)                                __NDBG_strdup_P(__VA_ARGS__)

#define __LDBG_IF(...)
#define __LDBG_N_IF(...)                                    __VA_ARGS__
#define __LDBG_S_IF(a, b)                                   b
#define __LMDBG_IF(...)
#define __LMDBG_N_IF(...)                                   __VA_ARGS__
#define __LMDBG_S_IF(a, b)                                  b
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
#include "debug_helper_disable_mem.h"
