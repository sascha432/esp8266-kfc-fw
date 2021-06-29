/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <pgmspace.h>
#include <algorithm>
#include <array>
#include <vector>
#include <stdint.h>
#include <functional>
#include <type_traits>
#include <utility>
#include <float.h>
#include <limits.h>

#if defined(ESP8266) || defined(ESP32) || _WIN32
#ifndef PROGMEM_DWORD_ALIGNED
#define PROGMEM_DWORD_ALIGNED                       1
typedef uint32_t                                    progmem_aligned_t;
#define PROGMEM_READ_ALIGNED_FUNC                   pgm_read_dword
#define PROGMEM_READ_ALIGNED_OFS(var)               uint8_t var = sizeof(progmem_aligned_t)
#define PROGMEM_READ_ALIGNED_CHUNK(var)             progmem_aligned_t var
#endif
#else
#define PROGMEM_READ_ALIGNED_FUNC                   pgm_read_byte
#define PROGMEM_READ_ALIGNED_OFS(var)
PROGMEM_READ_ALIGNED_CHUNK(var)
#endif

#include "WString.h"
#include "misc_string.h"
#include "misc_time.h"
#include "misc_safestring.h"

#ifdef __cplusplus
extern "C" {
#endif

    // internal
    extern const char *formatTimeNames_long[] PROGMEM;
    extern const char *formatTimeNames_short[] PROGMEM;

#ifdef __cplusplus
}
#endif


//class String;
using StringVector = std::vector<String>;

// pretty format for bytes and unix time
String formatBytes(size_t bytes);

//
// example
//  1 day 05:23:42
//
String formatTime(unsigned long seconds, bool printDaysIfZero = false);

// internal
String __formatTime(PGM_P names[], bool isShort, const String &sep, const String &_lastSep, bool displayZero, int seconds, int minutes = -1, int hours = -1, int days = -1, int weeks = -1, int months = -1, int years = -1, int milliseconds = -1, int microseconds = -1);

//
// create a string out of time
//
// formatTimeLong(F(", "), F(" and "), false, 0, 6, 5, 4, 0, 3, 2, 1)
// 1 year, 2 months, 3 days, 5 hours and 6 minutes
//
// if lastSep is an empty string, sep will be used
// displayZero if set to true, values with 0 will be displayed and values below 0 can be used to skip
//
static inline String formatTimeLong(const String &sep, const String &_lastSep, bool displayZero, int seconds, int minutes = -1, int hours = -1, int days = -1, int weeks = -1, int months = -1, int years = -1, int milliseconds = -1, int microseconds = -1)
{
    return __formatTime(formatTimeNames_long, false, sep, _lastSep,  displayZero, seconds, minutes, hours, days, weeks, months, years, milliseconds, microseconds);
}

// see formatTimeLong
// 1 yr, 2 mths, 3 days, 5 hrs and 6 min
static inline String formatTimeShort(const String &sep, const String &_lastSep, bool displayZero, int seconds, int minutes = -1, int hours = -1, int days = -1, int weeks = -1, int months = -1, int years = -1, int milliseconds = -1, int microseconds = -1)
{
    return __formatTime(formatTimeNames_short, true, sep, _lastSep,  displayZero, seconds, minutes, hours, days, weeks, months, years, milliseconds, microseconds);
}

static inline String formatTimeMicros(const String &sep, const String &lastSep, int seconds, int milliseconds, int microseconds)
{
    return formatTimeLong(sep, lastSep, false, seconds, -1, -1, -1, -1, -1, -1, milliseconds, microseconds);
}

static inline String formatTimeMicrosShort(const String &sep, const String &lastSep, int seconds, int milliseconds, int microseconds)
{
    return formatTimeShort(sep, lastSep, false, seconds, -1, -1, -1, -1, -1, -1, milliseconds, microseconds);
}

String url_encode(const String &str);
void printable_string(Print &output, const uint8_t *buffer, size_t length, size_t maxLength = 0, PGM_P extra = nullptr, bool crlfAsText = false);
String printable_string(const uint8_t *buffer, size_t length, size_t maxLength = 0, PGM_P extra = nullptr, bool crlfAsText = false);

static inline void printable_string(Print &output, const void *buffer, size_t length, size_t maxLength = 0, PGM_P extra = nullptr, bool crlfAsText = false) {
    return printable_string(output, reinterpret_cast<const uint8_t *>(buffer), length, maxLength, extra, crlfAsText);
}
static inline String printable_string(const void *buffer, size_t length, size_t maxLength = 0, PGM_P extra = nullptr, bool crlfAsText = false) {
    return printable_string(reinterpret_cast<const uint8_t *>(buffer), length, maxLength, extra, crlfAsText);
}

void append_slash(String &dir);
void remove_trailing_slash(String &dir);

const __FlashStringHelper *sys_get_temp_dir();
File tmpfile(String dir, const String &prefix);

const __FlashStringHelper *WiFi_disconnect_reason(WiFiDisconnectReason reason);

size_t _printMacAddress(const uint8_t *mac, Print &output, char separator = ':');

// nullptr safe
inline size_t printMacAddress(const void *mac, Print &output, char separator = ':') {
    return _printMacAddress(reinterpret_cast<const uint8_t *>(mac), output, separator);
}

// nullptr safe
String _mac2String(const uint8_t *mac, char separator = ':');

inline String mac2String(const void *mac, char separator = ':') {
    return _mac2String(reinterpret_cast<const uint8_t *>(mac), separator);
}

#define inet_ntoString(ip)                     IPAddress(ip).toString()
#define inet_nto_cstr(ip)                      IPAddress(ip).toString().c_str()

void bin2hex_append(String &str, const void *data, size_t length);
size_t hex2bin(void *buf, size_t length, const char *str);

// memcpy with the minimum size of both variables
#define MEMNCPY_S(dst, src)                     memcpy((void *)dst, (void *)src, std::min(sizeof(src), sizeof(dst)));

// interface
class TokenizerArgs {
public:
    TokenizerArgs() {
    }

    virtual void add(char *arg) = 0;
    virtual bool hasSpace() const = 0;
};

class TokenizerArgsCharPtrPtr : public TokenizerArgs {
public:
    TokenizerArgsCharPtrPtr(char** args, uint8_t maxArgs) : _args(args), _maxArgs(maxArgs), _count(0) {
    }

    virtual void add(char* arg) {
        _args[_count++] = arg;
    }
    virtual bool hasSpace() const {
        return (_count < _maxArgs);
    }

private:
    char** _args;
    uint8_t _maxArgs;
    uint8_t _count;
};

template <class T>
class TokenizerArgsVector : public TokenizerArgs {
public:
    TokenizerArgsVector(std::vector<T>& vector) : _vector(vector) {
    }

    virtual void add(char* arg) {
        _vector.push_back(arg);
    }
    virtual bool hasSpace() const {
        return true;
    }

    std::vector<T> &get() {
        return _vector;
    }

private:
    std::vector<T> &_vector;
};

// parse arguments into tokens
// return value == maxArgs might indicate that arguments have been truncated
// str='command=t1,t2,t3,"t3-1,t3-2", "t4-1""t4-2" , "t5-1\"t5-2\t5-3\\t5-4"[;next_command=...]'
// 0='t1'
// 1='t2'
// 2='t3'
// 3='t3-1,t3-2'
// 4='t4-1"t4-2'
// 5='t5-1"t5-2\t5-3\t5-4'
// 6="t6","t7";7="t8","t9"              multiple commands in one line


typedef std::function<bool(char ch, int type)> TokenizerSeparatorCallback;

uint16_t tokenizer(char *str, TokenizerArgs &args, bool hasCommand, char **nextCommand, TokenizerSeparatorCallback = nullptr);

#define repeat_first_iteration \
    (__repeat_iteration == 0)
#define repeat_last_iteration \
    (__repeat_iteration == (__repeat_count - 1))
#define repeat_func(count, ...) { \
        const auto  __repeat_count = count; \
        for(auto __repeat_iteration = 0; __repeat_iteration < __repeat_count; ++__repeat_iteration) { \
            __VA_ARGS__; \
        } \
    }

// storage for PROGMEM strings
// auto cast to const __FlashStringHelper *
// read characters with charAt or operator []
// get pointer to __FlashStringHelper with fp_str() or fp_str(index)

class FPStr {
public:
    FPStr(PGM_P str) : _str(str) {}
    FPStr(const __FlashStringHelper *str) : _str(str) {}
    // the String object must still exist if the string is accessed
    FPStr(const String &str) : _str(str.c_str()) {}

    explicit operator const char *() const {
        return reinterpret_cast<PGM_P>(_str);
    }
    operator const __FlashStringHelper *() const {
        return reinterpret_cast<const __FlashStringHelper *>(_str);
    }

    char charAt(int index) const {
        return (char)pgm_read_byte(c_str() + index);
    }

    char operator[](int index) const {
        return charAt(index);
    }

    PGM_P c_str() const {
        return reinterpret_cast<PGM_P>(_str);
    }

    const __FlashStringHelper *fp_str() const {
        return reinterpret_cast<const __FlashStringHelper *>(_str);
    }

    const __FlashStringHelper *fp_str(int index) const {
        return reinterpret_cast<const __FlashStringHelper *>(&c_str()[index]);
    }

private:
    const void *_str;
};

class StringFromBuffer : public String
{
public:
    StringFromBuffer(const char *str, size_t size) : String() {
        concat(str, size);
    }
    StringFromBuffer(const uint8_t *str, size_t size) : String() {
        concat(str, size);
    }
    StringFromBuffer(const __FlashStringHelper *str, size_t size) : String() {
        concat(str, size);
    }

    unsigned char concat(const void *str, size_t size) {
#if _MSC_VER
        return String::concat((const char *)str, size);
#else
        auto len = length();
        if (reserve(len + size)) {
            auto buf = begin();
            memmove_P(buf + len, str, size);
            len += size;
            setLen(len);
            buf[len] = 0;
            return 1;
        }
        return 0;
#endif
    }
};

namespace split {

    typedef enum {
        NONE =          0x00,
        EMPTY =         0x01,       // add empty strings
        TRIM =          0x02,       // trim strings
        _PROGMEM =      0x04,
    } SplitFlagsType;

    using AddItemCallback = std::function<void(const char *str, size_t len, int flags)>;

    template<typename T>
    void vector_callback(T strPtr, size_t len, int flags, StringVector &container)
    {
        String str = StringFromBuffer(strPtr, len);
        if (flags & SplitFlagsType::TRIM) {
            str.trim();
        }
        if ((flags & SplitFlagsType::EMPTY) || str.length()) {
            std::back_inserter(container) = std::move(str);
        }
    }

    void split(const char *str, const char *sep, AddItemCallback callback, int flags = SplitFlagsType::EMPTY, uint16_t limit = UINT16_MAX);
    void split_P(const FPStr &str, const FPStr &sep, AddItemCallback callback, int flags = SplitFlagsType::EMPTY, uint16_t limit = UINT16_MAX);
};

template<typename _Ta, typename _Tb, typename _Tc = String>
String implode(_Ta glue, const _Tb &pieces, size_t max = ~0) {
    String tmp;
    auto iterator = pieces.begin();
    if (max-- && iterator != pieces.end()) {
        tmp += _Tc(*iterator);
        while(max-- && ++iterator != pieces.end()) {
            tmp += glue;
            tmp += _Tc(*iterator);
        }
    }
    return tmp;
}

template<typename _Ta, typename _Tb, typename _Tc>
String implode_cb(_Ta glue, const _Tb &pieces, _Tc toString, size_t max = ~0) {
    String tmp;
    if (max-- && pieces.begin() != pieces.end()) {
        auto iterator = pieces.begin();
        tmp += toString(*iterator);
        while(max-- && ++iterator != pieces.end()) {
            tmp += glue;
            tmp += toString(*iterator);
        }
    }
    return tmp;
}

inline void explode(const char *str, const char *sep, StringVector &container, uint16_t limit = UINT16_MAX) {
    split::split(str, sep, [&container](const char *str, size_t size, int flags) {
        split::vector_callback(str, size, flags, container);
    }, split::SplitFlagsType::EMPTY, limit);
}

inline void explode_P(PGM_P str, PGM_P sep, StringVector &container, uint16_t limit = UINT16_MAX) {
    split::split_P(str, sep, [&container](const char *str, size_t size, int flags) {
        split::vector_callback(FPSTR(str), size, flags, container);
    }, split::SplitFlagsType::EMPTY, limit);
}

inline void explode(const char *str, char sep, StringVector &container, uint16_t limit = UINT16_MAX) {
    char buf[2] = { sep, 0 };
    explode(str, buf, container, limit);
}

inline void explode_P(PGM_P str, char sep, StringVector &container, uint16_t limit = UINT16_MAX) {
    char buf[2] = { sep, 0 };
    explode_P(str, buf, container, limit);
}

template <class T>
void *lambda_target(T callback) {
    if (callback) {
        auto addr = *(void **)((uint8_t *)(&callback) + 0); // if this points to flash it is a static function otherwise a mem function
        if ((const uintptr_t)addr <= 0x40000000) {
            return *(void **)((uint8_t *)(&callback) + 12); // this is the entry point of the mem function
        }
        return addr;
    }
    return 0;
}

// timezone support

// auto address = convertToIPAddress("192.168.0.1");
// if (IPAddress_isValid(address)) { //we can use the address }


// use instead of address.isSet()
// performs additional checks to validate the stored IP address
// some versions of the framework had issues doing it correctly
static inline bool IPAddress_isValid(const IPAddress &address) {
    return address != (uint32_t)0 && address != (uint32_t)0xffffffffU && address.isSet();
}

IPAddress convertToIPAddress(const char *hostname);

inline IPAddress convertToIPAddress(const String &hostname) {
    return convertToIPAddress(hostname.c_str());
}

#define __PP_CAT(a, b)                  __PP_CAT_I(a, b)
#define __PP_CAT_I(a, b)                __PP_CAT_II(~, a ## b)
#define __PP_CAT_II(p, res)             res
#define __UNIQUE_NAME(base)             __PP_CAT(__PP_CAT(__PP_CAT(base, __COUNTER__), _), __LINE__)

uint8_t numberOfSetBits(uint32_t i);
uint8_t numberOfSetBits(uint16_t i);

// #include "BitsToStr.h"

constexpr size_t kNumBitsRequired(int value, int n = 0) {
	return value ? kNumBitsRequired(value >> 1, n + 1) : n;
}

template<typename T>
constexpr size_t kNumBitsRequired(T value) {
    return kNumBitsRequired((int)value, 0);
}

template<typename T>
constexpr size_t kNumBitsRequired() {
    return kNumBitsRequired((int)T::MAX - 1, 0);
}

constexpr size_t _kGetRequiredBitsForValue(uint64_t value, size_t count = 0);

constexpr size_t _kGetRequiredBitsForValue(uint64_t value, size_t count) {
    return value == 0 ? count : _kGetRequiredBitsForValue(value >> 1, count + 1);
}

// get number of bits to store value
constexpr size_t kGetRequiredBitsForValue(int64_t value, size_t count) {
    return value == 0 ? count : value < 0 ? _kGetRequiredBitsForValue(static_cast<uint64_t>(-value), 1) : _kGetRequiredBitsForValue(value >> 1, count + 1);
}

// get number of bits to store values between from and to
constexpr size_t kGetRequiredBitsForRange(int64_t from, int64_t to) {
    return from < 0 || to < 0 ?
        std::max(_kGetRequiredBitsForValue(static_cast<uint64_t>(-std::min<int64_t>(from, to)), 1), _kGetRequiredBitsForValue(-from + to + 1, from <= 0 && to <= 0 ? 0 : 1)) :
        std::max(_kGetRequiredBitsForValue(std::max<uint64_t>(from, to), 0), _kGetRequiredBitsForValue(from + to));
}

// get number of bits for values from 0 to (num - offset)
constexpr size_t kGetRequiredBitsWithOffset(uint64_t offset, uint64_t num) {
    return offset > num ? _kGetRequiredBitsForValue(offset - num, 1) : _kGetRequiredBitsForValue(num - offset);
}

constexpr uint32_t kCreateIPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return a | (b << 8U) | (c << 16U) | (d << 24U);
}

inline uint32_t createIPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return a | (b << 8U) | (c << 16U) | (d << 24U);
}

#define CREATE_ENUM_BITFIELD_SIZE(name, size, enum_type, underlying_type, underlying_type_name) \
    underlying_type name: size; \
    static constexpr size_t kBitCountFor_##name = size; \
    inline static void set_enum_##name(Type &obj, enum_type value) { \
        obj.name = static_cast<underlying_type>(value); \
    } \
    inline void _set_enum_##name(enum_type value) { \
        name = static_cast<underlying_type>(value); \
    } \
    inline static enum_type get_enum_##name(const Type &obj) { \
        return static_cast<enum_type>(obj.name); \
    } \
    inline static void set_##underlying_type_name##_##name(Type &obj, underlying_type value) { \
        obj.name = value; \
    } \
    inline static void set_bits_##name(Type &obj, underlying_type value) { \
        obj.name = value; \
    } \
    inline static underlying_type get_##underlying_type_name##_##name(const Type &obj) { \
        return obj.name; \
    } \
    inline static underlying_type get_bits_##name(const Type &obj) { \
        return obj.name; \
    } \
    underlying_type _get_##underlying_type_name##_##name() const { \
        return name; \
    } \
    inline enum_type _get_enum_##name() const { \
        return static_cast<enum_type>(name); \
    } \
    inline static underlying_type cast_##underlying_type_name##_##name(enum_type value) { \
        return static_cast<underlying_type>(value); \
    } \
    inline static enum_type cast_enum_##name(underlying_type value) { \
        return static_cast<enum_type>(value); \
    } \
    inline static underlying_type cast(enum_type value) { \
        return static_cast<underlying_type>(value); \
    } \
    inline static enum_type cast_##name(underlying_type value) { \
        return static_cast<enum_type>(value); \
    }

#define CREATE_ENUM_BITFIELD_SIZE_DEFAULT(name, size, enum_type, underlying_type, underlying_type_name, default_value) \
    CREATE_ENUM_BITFIELD_SIZE(name, size, enum_type, underlying_type, underlying_type_name); \
    static constexpr auto kDefaultValueFor_##name = static_cast<underlying_type>(default_value);


#define CREATE_BITFIELD_TYPE(name, size, type, prefix) \
    type name: size; \
    static constexpr size_t kBitCountFor_##name = size; \
    inline static void set_##prefix##_##name(Type &obj, type value) { \
        obj.name = value; \
    } \
    inline static type get_##prefix##_##name(const Type &obj) { \
        return obj.name; \
    } \
    inline void _set_##name(type value) { \
        name = value; \
    } \
    inline type _get_##name() const { \
        return name; \
    }

#define CREATE_GETTER_SETTER_TYPE(name, size, type, prefix)  BOOST_PP_CAT(CREATE_GETTER_SETTER_TYPE_,prefix)(name, size, type, prefix)

#define CREATE_GETTER_SETTER_TYPE_int(name, size, type, prefix) \
    static constexpr size_t kBitCountFor_##name = size; \
    inline static void set_##prefix##_##name(Type &obj, type value) { \
        obj.name = value; \
    } \
    inline static type get_##prefix##_##name(const Type &obj) { \
        return obj.name; \
    } \
    inline static void set_bits_##name(Type &obj, type value) { \
        obj.name = value; \
    } \
    inline static type get_bits_##name(const Type &obj) { \
        return obj.name; \
    }

#define CREATE_GETTER_SETTER_TYPE_bit(name, size, type, prefix) CREATE_GETTER_SETTER_TYPE_int(name, size, type, prefix)

#define CREATE_GETTER_SETTER_TYPE_bits(name, size, type, prefix) \
    static constexpr size_t kBitCountFor_##name = size; \
    inline static void set_##prefix##_##name(Type &obj, type value) { \
        obj.name = value; \
    } \
    inline static type get_##prefix##_##name(const Type &obj) { \
        return obj.name; \
    }

// Type must be defined inside the struct/class
// using Type = MyStructure_t;

// requires last value MAX to be defined: enum class uint8_t { NONE, VAL1, VAL2, MAX };
#define CREATE_ENUM_D_BITFIELD(name, enum_type, def_val)    CREATE_ENUM_BITFIELD_SIZE_DEFAULT(name, kNumBitsRequired((int)enum_type::MAX - 1), enum_type, std::underlying_type<enum_type>::type, int, def_val)
#define CREATE_ENUM_BITFIELD(name, enum_type)               CREATE_ENUM_BITFIELD_SIZE(name, kNumBitsRequired((int)enum_type::MAX - 1), enum_type, std::underlying_type<enum_type>::type, int)
#define CREATE_ENUM_N_BITFIELD(name, size, enum_type)       CREATE_ENUM_BITFIELD_SIZE(name, size, enum_type, std::underlying_type<enum_type>::type, int)

#define CREATE_BOOL_BITFIELD(name)                          CREATE_BITFIELD_TYPE(name, 1, bool, bit)

#define CREATE_UINT8_BITFIELD(name, size)                   CREATE_BITFIELD_TYPE(name, size, uint8_t, bits)
#define CREATE_INT16_BITFIELD(name, size)                   CREATE_BITFIELD_TYPE(name, size, int16_t, bits)
#define CREATE_UINT16_BITFIELD(name, size)                  CREATE_BITFIELD_TYPE(name, size, uint16_t, bits)
#define CREATE_INT32_BITFIELD(name, size)                   CREATE_BITFIELD_TYPE(name, size, int32_t, bits)
#define CREATE_UINT32_BITFIELD(name, size)                  CREATE_BITFIELD_TYPE(name, size, uint32_t, bits)
#define CREATE_UINT64_BITFIELD(name, size)                  CREATE_BITFIELD_TYPE(name, size, uint64_t, bits)


#if ESP8266

struct Pins {
    union {
        uint32_t _value;
        struct {
            uint32_t GPIO0: 1;
            uint32_t GPIO1: 1;
            uint32_t GPIO2: 1;
            uint32_t GPIO3: 1;
            uint32_t GPIO4: 1;
            uint32_t GPIO6: 1;
            uint32_t GPIO7: 1;
            uint32_t GPIO8: 1;
            uint32_t GPIO9: 1;
            uint32_t GPIO10: 1;
            uint32_t GPIO11: 1;
            uint32_t GPIO12: 1;
            uint32_t GPIO13: 1;
            uint32_t GPIO14: 1;
            uint32_t GPIO15: 1;
            uint32_t GPIO16: 1;
        };
    };
    Pins() : _value(0) {}
    Pins(uint32_t value) : _value(value) {}
    operator int() const {
        return _value;
    }
};

inline static Pins digitalReadAll()
{
    return Pins(GPI | (GP16I ? (1U << 16) : 0));
}

#elif ESP32

#error TODO

// inline static uint64_t digitalReadAll() {
//     return 0;
// }

#else

inline static uint32_t digitalReadAll() {
    return 0;
}

#endif
