/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef TYPENAME_HAVE_PROGMEM_NAME
#define TYPENAME_HAVE_PROGMEM_NAME                      1
#endif

#ifndef TYPENAME_HAVE_PROGMEM_PRINT_FORMAT
#define TYPENAME_HAVE_PROGMEM_PRINT_FORMAT              0
#endif

enum class TypeNameEnum : uint8_t {
    INVALID = 0,
    CHAR_PTR,
    CONST_CHAR_PTR,
    VOID_PTR,
    CONST_VOID_PTR,
    VOID_T,
    CHAR_T,
    INT8_T,
    UINT8_T,
    INT16_T,
    UINT16_T,
    INT32_T,
    UINT32_T,
    INT64_T,
    UINT64_T,
    LONG,
    ULONG,
    FLOAT,
    DOUBLE,
    BOOL,
    FPSTR,
    STRING,
    NULLPTR_T,
    UINT8_PTR,
    CONST_UINT8_PTR,
};

template<class T>
struct constexpr_TypeName {
    using Type = struct {};
    using PrintCast = Type;
    constexpr static bool valid() { return false; }
    constexpr static const char *name() { return nullptr; }
    constexpr static const char *printFormat() { return nullptr; }
    constexpr static TypeNameEnum id() { return TypeNameEnum::INVALID; }
};

template<int typeId>
struct constexpr_TypeNameId {
    using Type = struct {};
    using PrintCast = Type;
    constexpr static bool valid() { return false; }
    constexpr static const char *name() { return nullptr; }
    constexpr static const char *printFormat() { return nullptr; }
    constexpr static TypeNameEnum id() { return TypeNameEnum::INVALID; }
};


#if TYPENAME_HAVE_PROGMEM_NAME
extern const char *TypeName_name[] PROGMEM;
const __FlashStringHelper *TypeName_getName(TypeNameEnum id);
#endif

#if TYPENAME_HAVE_PROGMEM_PRINT_FORMAT
extern const char *TypeName_format[] PROGMEM;
const __FlashStringHelper *TypeName_getPrintFormat(TypeNameEnum id);
#endif

#define ENABLE_TYPENAME(ID, TYPE, FORMAT, PRINT_CAST) \
    template<> \
    struct constexpr_TypeName<TYPE> { \
        using Type = TYPE; \
        using PrintCast = PRINT_CAST; \
        constexpr static bool valid() { return true; } \
        constexpr static const char *name() { return #TYPE; } \
        constexpr static const char *printFormat() { return #FORMAT; } \
        constexpr static TypeNameEnum id() { return static_cast<TypeNameEnum>(ID); }; \
    }; \
    template<> \
    struct constexpr_TypeNameId<ID> { \
        using Type = TYPE; \
        using PrintCast = PRINT_CAST; \
        constexpr static bool valid() { return true; } \
        constexpr static const char *name() { return #TYPE; } \
        constexpr static const char *printFormat() { return #FORMAT; } \
        constexpr static TypeNameEnum id() { return static_cast<TypeNameEnum>(ID); }; \
    };

#if defined(ESP8266) || defined(ESP32)
#define __TYPENAME_INT64_CAST               double
#define __TYPENAME_INT64_FORMAT             "%.0f"
#define __TYPENAME_UINT64_CAST              double
#define __TYPENAME_UINT64_FORMAT            "%.0f"
#define __TYPENAME_PROGMEM_CAST             PGM_P
#define __TYPENAME_PROGMEM_FORMAT           "%s"
#define __TYPENAME_STRING_CAST              const void *&
#define __TYPENAME_STRING_FORMAT            "%p"
#else
#define __TYPENAME_INT64_CAST               int64_t
#define __TYPENAME_INT64_FORMAT             "%lld"
#define __TYPENAME_UINT64_CAST              uint64_t
#define __TYPENAME_UINT64_FORMAT            "%llu"
#define __TYPENAME_PROGMEM_CAST             const char *
#define __TYPENAME_PROGMEM_FORMAT           "%s"
#define __TYPENAME_STRING_CAST              const char *
#define __TYPENAME_STRING_FORMAT            "%s"
#endif

#define ENABLE_TYPENAME_ALL() \
    ENABLE_TYPENAME(1, char *, "%s", const char *); \
    ENABLE_TYPENAME(2, const char *, "%s", const char *); \
    ENABLE_TYPENAME(3, void *, "%p", const void *); \
    ENABLE_TYPENAME(4, const void *, "%p", const void *); \
    ENABLE_TYPENAME(5, void, "%p", const void *&); \
    ENABLE_TYPENAME(6, char, "%c", int); \
    ENABLE_TYPENAME(7, int8_t, "%d", int); \
    ENABLE_TYPENAME(8, uint8_t, "%u", unsigned); \
    ENABLE_TYPENAME(9, int16_t, "%d", unsigned); \
    ENABLE_TYPENAME(10, uint16_t, "%u", unsigned); \
    ENABLE_TYPENAME(11, int32_t, "%d", int); \
    ENABLE_TYPENAME(12, uint32_t, "%u", unsigned); \
    ENABLE_TYPENAME(13, int64_t, __TYPENAME_INT64_FORMAT, __TYPENAME_INT64_CAST); \
    ENABLE_TYPENAME(14, uint64_t, __TYPENAME_UINT64_FORMAT, __TYPENAME_UINT64_CAST); \
    ENABLE_TYPENAME(15, long, "%ld", long); \
    ENABLE_TYPENAME(16, unsigned long, "%lu", unsigned long); \
    ENABLE_TYPENAME(17, float, "%f", double); \
    ENABLE_TYPENAME(18, double, "%f", double); \
    ENABLE_TYPENAME(19, bool, "%u", int); \
    ENABLE_TYPENAME(20, const __FlashStringHelper *, __TYPENAME_PROGMEM_FORMAT, __TYPENAME_PROGMEM_CAST); \
    ENABLE_TYPENAME(21, String, __TYPENAME_STRING_FORMAT, __TYPENAME_STRING_CAST); \
    ENABLE_TYPENAME(22, nullptr_t, "%p", const void *); \
    ENABLE_TYPENAME(23, uint8_t *, "%p", const void *); \
    ENABLE_TYPENAME(24, const uint8_t *, "%p", const void *);

ENABLE_TYPENAME_ALL();
