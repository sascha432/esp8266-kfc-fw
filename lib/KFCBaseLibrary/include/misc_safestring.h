/**
  Author: sascha_lammers@gmx.de
*/

// macros:
// __S(ptr)     -> const char *
// __SL(ptr)    -> strlen(ptr)
//
// compile time detection for printf "%s" to output. the return type is always const char * and
// might point to PROGMEM. requires to use PROGMEM safe functions and access memory with 32bit
// alignment. consider using PGM_read_* functions
//
// const char *
// PGM_P
// String
// IPAddress
//
// nullptr and invalid pointers are detected during runtime

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if _MSC_VER
    extern const char *SPGM_null PROGMEM;
    extern const char *SPGM_0x_08x PROGMEM;
#else
    extern const char SPGM_null[] PROGMEM;
    extern const char SPGM_0x_08x[] PROGMEM;
#endif

#if ESP8266
    extern char _heap_start[];
#endif

#ifdef __cplusplus
}
#endif

#if ESP8266

#include "umm_malloc/umm_malloc.h"

#define PROGMEM_START_ADDRESS                               0x40200000U
#define PROGMEM_END_ADDRESS                                 0x402FEFF0U
#define HEAP_START_ADDRESS                                  ((uintptr_t)&_heap_start[0])
#define HEAP_END_ADDRESS                                    UMM_HEAP_END_ADDR

inline bool is_HEAP_P(const void *ptr) {
    return (reinterpret_cast<const uintptr_t>(ptr) >= HEAP_START_ADDRESS) && (reinterpret_cast<const uintptr_t>(ptr) < HEAP_END_ADDRESS);
}

inline bool is_HEAP_P(const void *begin, const void *end) {
    return is_HEAP_P(begin) && is_HEAP_P(end);
}

inline bool is_PGM_P(const void *ptr) {
    return (reinterpret_cast<const uintptr_t>(ptr) >= PROGMEM_START_ADDRESS) && (reinterpret_cast<const uintptr_t>(ptr) < PROGMEM_END_ADDRESS);
}

inline bool is_PGM_P(const void *begin, const void *end) {
    return is_PGM_P(begin) && is_PGM_P(end);
}

inline bool is_aligned_PGM_P(const void * ptr)
{
    return ((reinterpret_cast<const uintptr_t>(ptr) & 3) == 0);
}

inline bool is_not_PGM_P_or_aligned(const void * ptr)
{
    return !is_PGM_P(ptr) || is_aligned_PGM_P(ptr);
}



#define __ASSERT_PTR(ptr)               (is_HEAP_P(ptr) || is_PGM_P(ptr) ? true : __DBG_printf("INVALID PTR %p", ptr));
#define __IS_SAFE_STR(str)              (is_HEAP_P(str) || is_PGM_P(str) ? str : __safeCString((const void *)str).c_str())

#else

bool is_PGM_P(const void *ptr);

inline bool is_aligned_PGM_P(const void * ptr)
{
    return (((const uintptr_t)ptr & 3) == 0);
}

inline bool is_not_PGM_P_or_aligned(const void * ptr)
{
    return is_aligned_PGM_P(ptr);
}

#define __ASSERT_PTR(str)               ;
#define __IS_SAFE_STR(str)              str

#endif

// __S(str) for printf_P("%s") or any function requires const char * arguments
//
// const char *str                       __S(str) = str
// const __FlashStringHelper *str        __S(str) = (const char *)str
// String test;                         __S(test) = test.c_str()
// on the stack inside the a function call. the object gets destroyed when the function returns
//                                      __S(String()) = String().c_str()
// IPAddress addr;                       __S(addr) = addr.toString().c_str()
//                                      __S(IPAddress()) = IPAddress().toString().c_str()
// nullptr_t or any nullptr             __S((const char *)0) = "null"
// const void *                         __S(const void *)1767708) = String().printf("0x%08x", 0xff).c_str() = 0x001af91c

#define _S_STRLEN(str)                  (str ? strlen_P(__S(str)) : 0)
#define _S_STR(str)                     __S(str)
#define __S(str)                        __safeCString(str).c_str()
// #define __S(str)                        __safeCString::validateLength(__safeCString(str).c_str())

inline static const String __safeCString(const void *ptr) {
    char buf[16];
    snprintf_P(buf, sizeof(buf), SPGM(0x_08x), ptr);
    return buf;
}

class SafeStringWrapper {
public:
    const char *_str;

    constexpr SafeStringWrapper() : _str(SPGM(null)) {}
    constexpr SafeStringWrapper(const char *str) : _str(str ? __IS_SAFE_STR(str) : SPGM(null)) {}
    constexpr SafeStringWrapper(const __FlashStringHelper *str) : _str(str ? __IS_SAFE_STR((PGM_P)str) : SPGM(null)) {}
    constexpr const char *c_str() const {
        return _str;
    }

    // inline static bool validateLength(const char *str) {
    //     return true;
    // }

};

inline static const String __safeCString(const IPAddress &addr) {
    return addr.toString();
}

constexpr const String &__safeCString(const String &str) {
    return str;
}

constexpr const SafeStringWrapper __safeCString(const __FlashStringHelper *str) {
    return SafeStringWrapper(str);
}

constexpr const SafeStringWrapper __safeCString(const char *str) {
    return SafeStringWrapper(str);
}

constexpr const SafeStringWrapper __safeCString(nullptr_t ptr) {
    return SafeStringWrapper();
}
