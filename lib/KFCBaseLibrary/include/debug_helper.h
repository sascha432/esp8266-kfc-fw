/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if defined(HAVE_MEM_NO_DEBUG) || defined(KFC_DEVICE_ID)
#undef HAVE_MEM_DEBUG
#endif

#ifndef HAVE_MEM_DEBUG
#define HAVE_MEM_DEBUG                                      0
#endif


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
#define __NDBG_new_array(num, name, ...)                    new name[num](__VA_ARGS__)
#define __NDBG_delete_array(ptr)                            delete[] ptr
#define __NDBG_track_new(...)                               ;
#define __NDBG_track_delete(...)                            ;
#define __NDBG_malloc(size)                                 malloc(size)
#define __NDBG_realloc(ptr, size)                           realloc(ptr, size)

#define __NDBG_free(ptr)                                    free(ptr)
#define __NDBG_malloc_str(size)                             reinterpret_cast<char *>(__NDBG_malloc(size))
#define __NDBG_malloc_buf(size)                             reinterpret_cast<uint8_t *>(__NDBG_malloc(size))
#define __NDBG_realloc_str(str, size)                       reinterpret_cast<char *>(__NDBG_realloc(str, size))
#define __NDBG_realloc_buf(ptr, size)                       reinterpret_cast<uint8_t *>(__NDBG_realloc(ptr, size))
#define __NDBG_strdup(str)                                  strdup(str)
#define __NDBG_strdup_P(str)                                strdup_P(str)

#define __NDBG_NOP_new(...)                                 ;
#define __NDBG_NOP_delete(...)                              ;
#define __NDBG_NOP_new_array(...)                           ;
#define __NDBG_NOP_delete_array(...)                        ;
#define __NDBG_NOP_malloc(...)                              ;
#define __NDBG_NOP_realloc(...)                             ;
#define __NDBG_NOP_free(...)                                ;

// invokes panic() on ESP8266/32, calls __debugbreak() with visual studio to intercept and debug the error
extern int ___debugbreak_and_panic(const char *filename, int line, const char *function);

#define __debugbreak_and_panic()                            ___debugbreak_and_panic(__BASENAME_FILE__, __LINE__, __DEBUG_FUNCTION__)

#if DEBUG

extern const char ___debugPrefix[] PROGMEM;

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

// debug
#if HAVE_MEM_DEBUG
#include "DebugMemoryHelper.h"
#define __DBG_allocator                                     __DebugAllocator
#define __DBG_track_allocator                               __TrackAllocator
#define __DBG_operator_new(...)                             KFCMemoryDebugging::_new(DebugContext_ctor(), (__VA_ARGS__), __NDBG_operator_new(__VA_ARGS__))
#define __DBG_operator_delete(ptr)                          __NDBG_operator_delete(KFCMemoryDebugging::_delete(DebugContext_ctor(), ptr))
#define __DBG_track_new(size)                               KFCMemoryDebugging::_track_new(size)
#define __DBG_track_delete(size)                            KFCMemoryDebugging::_track_delete(nullptr, size)
#define __DBG_new(name, ...)                                KFCMemoryDebugging::_new(DebugContext_ctor(), sizeof(name), new name(__VA_ARGS__))
#define __DBG_delete(ptr)                                   delete KFCMemoryDebugging::_delete(DebugContext_ctor(), ptr)
#define __DBG_new_array(n, name, ...)                       KFCMemoryDebugging::_new(DebugContext_ctor(), n * sizeof(name), new name[n](__VA_ARGS__))
#define __DBG_delete_array(ptr)                             delete[] KFCMemoryDebugging::_delete(DebugContext_ctor(), ptr)
#define __DBG_malloc(size)                                  KFCMemoryDebugging::_alloc(DebugContext_ctor(), size, malloc(size))
#define __DBG_realloc(ptr, size)                            KFCMemoryDebugging::_realloc(DebugContext_ctor(), size, ptr, realloc(ptr, size))
#define __DBG_free(ptr)                                     free(KFCMemoryDebugging::_free(DebugContext_ctor(), ptr))

#define __DBG_malloc_str(size)                              reinterpret_cast<char *>(__DBG_malloc(size))
#define __DBG_malloc_buf(size)                              reinterpret_cast<uint8_t *>(__DBG_malloc(size))
#define __DBG_realloc_str(ptr, size)                        reinterpret_cast<char *>(__DBG_realloc(ptr, size))
#define __DBG_realloc_buf(ptr, size)                        reinterpret_cast<uint8_t *>(__DBG_realloc(ptr, size))
#define __DBG_strdup(str)                                   KFCMemoryDebugging::_alloc(DebugContext_ctor(), strdup(str))
#define __DBG_strdup_P(str)                                 KFCMemoryDebugging::_alloc(DebugContext_ctor(), strdup_P(str))

#define __DBG_NOP_new(ptr)                                  KFCMemoryDebugging::_new(DebugContext_ctor(), sizeof(*ptr), ptr)
#define __DBG_NOP_delete(ptr)                               KFCMemoryDebugging::_delete(DebugContext_ctor(), ptr)
#define __DBG_NOP_new_array(n, ptr)                         KFCMemoryDebugging::_new(DebugContext_ctor(), n * sizeof(*ptr), ptr)
#define __DBG_NOP_delete_array(ptr)                         KFCMemoryDebugging::_delete(DebugContext_ctor(), ptr)
#define __DBG_NOP_malloc(ptr, size)                         KFCMemoryDebugging::_alloc(DebugContext_ctor(), size, ptr)
#define __DBG_NOP_realloc(old_ptr, new_ptr, size)           KFCMemoryDebugging::_realloc(DebugContext_ctor(), size, old_ptr, new_ptr)
#define __DBG_NOP_free(ptr)                                 KFCMemoryDebugging::_free(DebugContext_ctor(), ptr)

#else

#define __DBG_allocator                                     __NDBG_allocator
#define __DBG_operator_new(...)                             __NDBG_operator_new(__VA_ARGS__)
#define __DBG_operator_delete(...)                          __NDBG_operator_delete(__VA_ARGS__)
#define __DBG_track_new(size)                               __NDBG_track_new(size)
#define __DBG_track_delete(size)                            __NDBG_track_delete(size)
#define __DBG_new(name, ...)                                __NDBG_new(name, ##__VA_ARGS__)
#define __DBG_delete(obj)                                   __NDBG_delete(obj)
#define __DBG_new_array(num, name, ...)                     __NDBG_new_array(num, name, ##__VA_ARGS__)
#define __DBG_delete_array(ptr)                             __NDBG_delete_array(ptr)
#define __DBG_malloc(size)                                  __NDBG_malloc(size)
#define __DBG_realloc(ptr, size)                            __NDBG_realloc(ptr, size)
#define __DBG_free(ptr)                                     __NDBG_free(ptr)

#define __DBG_malloc_str(size)                              __NDBG_malloc_str(size)
#define __DBG_malloc_buf(size)                              __NDBG_malloc_buf(size)
#define __DBG_realloc_str(ptr, size)                        __NDBG_realloc_str(ptr, size)
#define __DBG_realloc_buf(ptr, size)                        __NDBG_realloc_buf(ptr, size)
#define __DBG_strdup(str)                                   __NDBG_strdup(str)
#define __DBG_strdup_P(str)                                 __NDBG_strdup_P(str)

#define __DBG_NOP_new(arg, ...)                             __NDBG_NOP_new(arg, ##__VA_ARGS__)
#define __DBG_NOP_delete(arg, ...)                          __NDBG_NOP_delete(arg, ##__VA_ARGS__)
#define __DBG_NOP_new_array(arg, ...)                       __NDBG_NOP_new_array(arg, ##__VA_ARGS__)
#define __DBG_NOP_delete_array(arg, ...)                    __NDBG_NOP_delete_array(arg, ##__VA_ARGS__)
#define __DBG_NOP_malloc(arg, ...)                          __NDBG_NOP_malloc(arg, ##__VA_ARGS__)
#define __DBG_NOP_realloc(arg, ...)                         __NDBG_NOP_realloc(arg, ##__VA_ARGS__)
#define __DBG_NOP_free(arg, ...)                            __NDBG_NOP_free(arg, ##__VA_ARGS__)

#endif

// debug if local and memory debugging is enabled
#define __LDBG_allocator                                    __LMDBG_S_IF(__DBG_allocator, __NDBG_allocator)
#define __LDBG_track_allocator                              __LMDBG_S_IF(__DBG_track_allocator, __NDBG_allocator)
#define __LDBG_operator_new(arg, ...)                       __LMDBG_S_IF(__DBG_operator_new(arg, ##__VA_ARGS__), __NDBG_operator_new(arg, ##__VA_ARGS__))
#define __LDBG_operator_delete(arg, ...)                    __LMDBG_S_IF(__DBG_operator_delete(arg, ##__VA_ARGS__), __NDBG_operator_delete(arg, ##__VA_ARGS__))
#define __LDBG_track_new(size)                              __LMDBG_S_IF(__DBG_track_new(size), __NDBG_track_new(size))
#define __LDBG_track_delete(size)                           __LMDBG_S_IF(__DBG_track_delete(size), __NDBG_track_delete(size))
#define __LDBG_new(name, ...)                               __LMDBG_S_IF(__DBG_new(name, ##__VA_ARGS__), __NDBG_new(name, ##__VA_ARGS__))
#define __LDBG_delete(ptr)                                  __LMDBG_S_IF(__DBG_delete(ptr), __NDBG_delete(ptr))
#define __LDBG_new_array(num, name, ...)                    __LMDBG_S_IF(__DBG_new_array(num, name, ##__VA_ARGS__), __NDBG_new_array(num, name, ##__VA_ARGS__))
#define __LDBG_delete_array(ptr)                            __LMDBG_S_IF(__DBG_delete_array(ptr), __NDBG_delete_array(ptr))
#define __LDBG_malloc(size)                                 __LMDBG_S_IF(__DBG_malloc(size), __NDBG_malloc(size))
#define __LDBG_realloc(ptr, size)                           __LMDBG_S_IF(__DBG_realloc(ptr, size), __NDBG_realloc(ptr, size))
#define __LDBG_free(ptr)                                    __LMDBG_S_IF(__DBG_free(ptr), __NDBG_free(ptr))

#define __LDBG_malloc_str(size)                             __LMDBG_S_IF(__DBG_malloc_str(size), __NDBG_malloc_str(size))
#define __LDBG_malloc_buf(size)                             __LMDBG_S_IF(__DBG_malloc_buf(size), __NDBG_malloc_buf(size))
#define __LDBG_realloc_str(ptr, size)                       __LMDBG_S_IF(__DBG_realloc_str(ptr, size), __NDBG_realloc_str(ptr, size))
#define __LDBG_realloc_buf(ptr, size)                       __LMDBG_S_IF(__DBG_realloc_buf(ptr, size), __NDBG_realloc_buf(ptr, size))
#define __LDBG_strdup(str)                                  __LMDBG_S_IF(__DBG_strdup(str), __NDBG_strdup(str))
#define __LDBG_strdup_P(str)                                __LMDBG_S_IF(__DBG_strdup_P(str), __NDBG_strdup_P(str))

#define __LDBG_NOP_free(...)                                __LMDBG_S_IF(__DBG_NOP_free(__VA_ARGS__), __NDBG_NOP_free(__VA_ARGS__))
#define __LDBG_NOP_delete(...)                              __LMDBG_S_IF(__DBG_NOP_delete(__VA_ARGS__), __NDBG_NOP_delete(__VA_ARGS__))
#define __LDBG_NOP_new_array(...)                           __LMDBG_S_IF(__DBG_NOP_new_array(__VA_ARGS__), __NDBG_NOP_new_array(__VA_ARGS__))
#define __LDBG_NOP_delete_array(...)                        __LMDBG_S_IF(__DBG_NOP_delete_array(__VA_ARGS__), __NDBG_NOP_delete_array(__VA_ARGS__))
#define __LDBG_NOP_malloc(...)                              __LMDBG_S_IF(__DBG_NOP_malloc(__VA_ARGS__), __NDBG_NOP_malloc(__VA_ARGS__))
#define __LDBG_NOP_realloc(...)                             __LMDBG_S_IF(__DBG_NOP_realloc(__VA_ARGS__), __NDBG_NOP_realloc(__VA_ARGS__))
#define __LDBG_NOP_free(...)                                __LMDBG_S_IF(__DBG_NOP_free(__VA_ARGS__), __NDBG_NOP_free(__VA_ARGS__))


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

#define __DBG_function()                                    DebugContext::Guard guard(DebugContext_ctor());
#define __DBG_function_printf(fmt, ...)                     DebugContext::Guard guard(DebugContext_ctor(), PSTR(fmt), ##__VA_ARGS__);

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
#define __LDBG_S_IF(a, b)                                   __LDBG_IF(a)__LDBG_N_IF(b)
#define __LMDBG_S_IF(a, b)                                  __LMDBG_IF(a)__LMDBG_N_IF(b)
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
#   define                                                  __debug_prefix(stream) stream.printf_P(___debugPrefix, millis(), __BASENAME_FILE__, __LINE__, ESP.getFreeHeap(), can_yield(), __DEBUG_FUNCTION__)
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

// MSVC version

#ifndef _ASSERTE
#define _ASSERTE(...)
#endif
#ifndef _ASSERT_EXPR
#define _ASSERT_EXPR(...)
#endif

// new functions

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

// memory management

#define __DBG_allocator                                     __NDBG_allocator
#define __DBG_operator_new(arg, ...)                             __NDBG_operator_new(arg, ##__VA_ARGS__)
#define __DBG_operator_delete(arg, ...)                          __NDBG_operator_delete(arg, ##__VA_ARGS__)
#define __DBG_new(arg, ...)                                      __NDBG_new(arg, ##__VA_ARGS__)
#define __DBG_delete(arg, ...)                                   __NDBG_delete(arg, ##__VA_ARGS__)
#define __DBG_delete_remove(arg, ...)                            __NDBG_delete_remove(arg, ##__VA_ARGS__)
#define __DBG_new_array(arg, ...)                                __NDBG_new_array(arg, ##__VA_ARGS__)
#define __DBG_delete_array(arg, ...)                             __NDBG_delete_array(arg, ##__VA_ARGS__)
#define __DBG_malloc(arg, ...)                                   __NDBG_malloc(arg, ##__VA_ARGS__)
#define __DBG_realloc(arg, ...)                                  __NDBG_realloc(arg, ##__VA_ARGS__)
#define __DBG_free(arg, ...)                                     __NDBG_free(arg, ##__VA_ARGS__)
#define __DBG_malloc_str(arg, ...)                               __NDBG_malloc_str(arg, ##__VA_ARGS__)
#define __DBG_malloc_buf(arg, ...)                               __NDBG_malloc_buf(arg, ##__VA_ARGS__)
#define __DBG_strdup(arg, ...)                                   __NDBG_strdup(arg, ##__VA_ARGS__)
#define __DBG_strdup_P(arg, ...)                                 __NDBG_strdup_P(arg, ##__VA_ARGS__)

#define __LDBG_allocator                                    __NDBG_allocator
#define __LDBG_operator_new(arg, ...)                            __NDBG_operator_new(arg, ##__VA_ARGS__)
#define __LDBG_operator_delete(arg, ...)                         __NDBG_operator_delete(arg, ##__VA_ARGS__)
#define __LDBG_new(arg, ...)                                     __NDBG_new(arg, ##__VA_ARGS__)
#define __LDBG_delete(arg, ...)                                  __NDBG_delete(arg, ##__VA_ARGS__)
#define __LDBG_delete_remove(arg, ...)                           __NDBG_delete_remove(arg, ##__VA_ARGS__)
#define __LDBG_new_array(arg, ...)                               __NDBG_new_array(arg, ##__VA_ARGS__)
#define __LDBG_delete_array(arg, ...)                            __NDBG_delete_array(arg, ##__VA_ARGS__)
#define __LDBG_malloc(arg, ...)                                  __NDBG_malloc(arg, ##__VA_ARGS__)
#define __LDBG_realloc(arg, ...)                                 __NDBG_realloc(arg, ##__VA_ARGS__)
#define __LDBG_free(arg, ...)                                    __NDBG_free(arg, ##__VA_ARGS__)
#define __LDBG_malloc_str(arg, ...)                              __NDBG_malloc_str(arg, ##__VA_ARGS__)
#define __LDBG_malloc_buf(arg, ...)                              __NDBG_malloc_buf(arg, ##__VA_ARGS__)
#define __LDBG_strdup(arg, ...)                                  __NDBG_strdup(arg, ##__VA_ARGS__)
#define __lDBG_strdup_P(arg, ...)                                __NDBG_strdup_P(arg, ##__VA_ARGS__)

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
//#include "debug_helper_disable_mem.h"
#include "debug_helper_enable_mem.h"
