/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

template<class T>
struct TypeName {
    static const __FlashStringHelper *name() { return nullptr; }
    static const __FlashStringHelper *sname() { return nullptr; }
    static const __FlashStringHelper *format() { return nullptr; }
    static const char *name_cstr() { return nullptr; }
};

typedef struct {
    bool valid;
    const char *name;
    const char *short_name;
    const char *printf_format;
    const char *type;
} constexpr_TypeName_info_t;

template<class T>
struct constexpr_TypeName {
    constexpr static constexpr_TypeName_info_t info() { return { false }; }
};

#define ENABLE_TYPENAME(T, SNAME, FORMAT, NAME) \
extern const char TypeName_details_##NAME##name[] PROGMEM ; \
extern const char TypeName_details_##NAME##sname[] PROGMEM ; \
extern const char TypeName_details_##NAME##format[] PROGMEM ; \
template<> \
struct TypeName<T> { \
    static const __FlashStringHelper* name(); \
    static const __FlashStringHelper* sname(); \
    static const __FlashStringHelper* format(); \
    static const char* name_cstr(); \
}; \
template<> \
struct constexpr_TypeName<T> { \
    constexpr static constexpr_TypeName_info_t info() { return { true, #NAME, #SNAME, FORMAT, #T }; } \
};

#define ENABLE_TYPENAME_ALL() \
    ENABLE_TYPENAME(char *, char *, "%s", char_ptr); \
    ENABLE_TYPENAME(const char *, const char *, "%s", const_char_ptr); \
    ENABLE_TYPENAME(void *, void *, "%p", void_ptr); \
    ENABLE_TYPENAME(const void *, const void *, "%p", const_void_ptr); \
    ENABLE_TYPENAME(void, void, "%p", void); \
    ENABLE_TYPENAME(char, char, "%c", char); \
    ENABLE_TYPENAME(signed char, int8_t, "%d", signed_char); \
    ENABLE_TYPENAME(unsigned char, uint8_t, "%u", unsigned_char); \
    ENABLE_TYPENAME(short, int16_t, "%d", short); \
    ENABLE_TYPENAME(unsigned short, uint16_t, "%u", unsigned_short); \
    ENABLE_TYPENAME(int, int32_t, "%d", int); \
    ENABLE_TYPENAME(unsigned int, uint32_t, "%u", unsigned_int); \
    ENABLE_TYPENAME(long, long, "%ll", long); \
    ENABLE_TYPENAME(unsigned long, ulong, "%ull", unsigned_long); \
    ENABLE_TYPENAME(long long, int64_t, "%ll", long_long); \
    ENABLE_TYPENAME(unsigned long long, uint64_t, "%ull", unsigned_long_long); \
    ENABLE_TYPENAME(float, float, "%f", float); \
    ENABLE_TYPENAME(double, double, "%f", double); \
    ENABLE_TYPENAME(bool, bool, "%u", bool); \
    ENABLE_TYPENAME(const __FlashStringHelper *, FPSTR, "%s", fp_str); \
    ENABLE_TYPENAME(String, String, "%p", string); \
    ENABLE_TYPENAME(nullptr_t, nullptr, "%p", nullptr);

ENABLE_TYPENAME_ALL();
