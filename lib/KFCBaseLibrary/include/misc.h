/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <pgmspace.h>
#include <algorithm>
#include <array>
#include <vector>
#include <functional>
#include <type_traits>
#include <utility>
#include <array_of.h>
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

class String;
typedef std::vector<String> StringVector;

// #if !_WIN32

// // adds nconcat(c_str, max_len), for example to append a byte array to the string that isnt null terminated
// class StringEx : public String {
// public:
//     StringEx() : String() {
//     }
//     StringEx(const String &str) : String(str) {
//     }
//     StringEx(const char *cstr, size_t alen) : String() {
//         if (!reserve(alen)) {
//             invalidate();
//         } else {
// #if defined(ARDUINO_ESP8266_RELEASE_2_5_2) || defined(ARDUINO_ESP8266_RELEASE_2_6_3)
//             setLen(alen);
// #else
//             len = alen;
// #endif
// #if ESP32
//             strncpy(buffer, cstr, alen)[alen] = 0;
// #else
//             strncpy(begin(), cstr, alen)[alen] = 0;
// #endif
//         }
//     }

//     // void nconcat(const char *cstr, size_t alen) {
//     //     if (!reserve(start_length() + alen)) {
//     //         invalidate();
//     //     } else {
//     //         setLen(start_length() + alen);
//     //         strncpy(begin() + start_length(), cstr, alen)[alen] = 0;
//     //     }
//     // }
// };

// #endif

// pretty format for bytes and unix time
String formatBytes(size_t bytes);
String formatTime(unsigned long seconds, bool printDaysIfZero = false);

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
File tmpfile(const String &dir, const String &prefix);

String WiFi_disconnect_reason(WiFiDisconnectReason reason);

// copy data without NUL to a string object. truncated at len or first NUL
// NOTE src must not be a pointer
#define STRNCPY_TO_STRING(dst, src, src_len)    { \
        char buffer[sizeof(src) + 1]; \
        IF_DEBUG(if (src_len > sizeof(src) { debug_println("STRNCPY_TO_STRING(): len > sizeof(src)"); panic(); })); \
        strncpy(buffer, (char *)src, src_len)[src_len] = 0; \
        dst = buffer; \
    }

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

#define INET_NTOA_LEN                           16

String inet_ntoString(uint32_t ip);

const char *inet_ntoa_s(char *dst, size_t size, uint32_t ip);

#define inet_ntoa(dst, ip)                      inet_ntoa_s(dst, INET_NTOA_LEN, ip)

void bin2hex_append(String &str, const void *data, size_t length);
size_t hex2bin(void *buf, size_t length, const char *str);

// memcpy with the minimum size of both variables
#define MEMNCPY_S(dst, src)                     { memcpy((void *)dst, (void *)src, min(sizeof(src), sizeof(dst))); }

// bzero variant using sizeof(dst)
#define BNZERO_S(dst)                           memset(&dst, 0, sizeof(dst));

// NOTE:
// string functions are nullptr safe
// only PGM_P is PROGMEM safe

#define STRINGLIST_SEPARATOR                    ','
#define STRLS                                   ","

// find a string in a list of strings separated by a single characters
// -1 = not found, otherwise the number of the matching string
int stringlist_find_P_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR);

// multiple separators
int stringlist_find_P_P(PGM_P list, PGM_P find, PGM_P separator);

template<typename Tl, typename Tf, typename Tc>
inline int stringlist_find_P_P(Tl list, Tf find, Tc separator) {
    return stringlist_find_P_P(reinterpret_cast<PGM_P>(list), reinterpret_cast<PGM_P>(find), separator);
}

// compare two PROGMEM strings
int strcmp_P_P(PGM_P str1, PGM_P str2);

// compare two PROGMEM strings case insensitive
int strcasecmp_P_P(PGM_P str1, PGM_P str2);

int strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2);
inline int strcmp_end_P(const char *str1, size_t len1, PGM_P str2) {
    return strcmp_end_P(str1, len1, str2, strlen_P(str2));
}
inline int strcmp_end_P(const char *str1, PGM_P str2) {
    return strcmp_end_P(str1, strlen(str1), str2, strlen_P(str2));
}

// ends at maxLen characters or the first NUL byte
size_t str_replace(char *src, int from, int to, size_t maxLen = ~0);

// case insensitive comparision of from
size_t str_case_replace(char *src, int from, int to, size_t maxLen = ~0);

inline size_t String_replace(String &str, int from, int to) {
    return str_replace(str.begin(), from, to, str.length());
}

inline size_t String_replaceIgnoreCase(String &str, int from, int to) {
    return str_case_replace(str.begin(), from, to, str.length());
}

size_t String_rtrim(String &str);
size_t String_ltrim(String &str);
size_t String_trim(String &str);

size_t String_rtrim(String &str, const char *chars, size_t minLength = ~0);
size_t String_ltrim(String &str, const char *chars);
size_t String_trim(String &str, const char *chars);

size_t String_rtrim(String &str, char chars, size_t minLength = ~0);
size_t String_ltrim(String &str, char chars);
size_t String_trim(String &str, char chars);

size_t String_rtrim_P(String &str, PGM_P chars, size_t minLength = ~0);
size_t String_ltrim_P(String &str, PGM_P chars);
size_t String_trim_P(String &str, PGM_P chars);

inline size_t String_rtrim_P(String &str, const __FlashStringHelper *chars, size_t minLength = ~0) {
    return String_rtrim_P(str, reinterpret_cast<PGM_P>(chars), minLength);
}
inline size_t String_ltrim_P(String &str, const __FlashStringHelper *chars) {
    return String_ltrim_P(str, reinterpret_cast<PGM_P>(chars));
}
inline size_t String_trim_P(String &str, const __FlashStringHelper *chars) {
    return String_trim_P(str, reinterpret_cast<PGM_P>(chars));
}

// trim trailing zeros
// return length of the string
// output can be nullptr to get the length
size_t printTrimmedDouble(Print *output, double value, int digits = 6);

inline bool String_startsWith(const String &str1, char ch) {
    return str1.charAt(0) == ch;
}
bool String_endsWith(const String &str1, char ch);

// compare functions that do not create a String object of "str2"
bool String_startsWith(const String &str1, PGM_P str2);
bool String_endsWith(const String &str1, PGM_P str2);

// bool String_equals(const String &str1, PGM_P str2);
// bool String_equalsIgnoreCase(const String &str1, PGM_P str2);

inline bool String_equals(const String &str1, PGM_P str2) {
    return !strcmp_P(str1.c_str(), str2);
}
inline bool String_equalsIgnoreCase(const String &str1, PGM_P str2) {
    return !strcasecmp_P(str1.c_str(), str2);
}

inline bool String_equals(const String &str1, const __FlashStringHelper *str2) {
    return String_equals(str1, reinterpret_cast<PGM_P>(str2));
}
inline bool String_equalsIgnoreCase(const String &str1, const __FlashStringHelper *str2) {
    return String_equalsIgnoreCase(str1, reinterpret_cast<PGM_P>(str2));
}
inline bool String_startsWith(const String &str1, const __FlashStringHelper *str2) {
    return String_startsWith(str1, reinterpret_cast<PGM_P>(str2));
}
inline bool String_endsWith(const String &str1, const __FlashStringHelper *str2) {
    return String_endsWith(str1, reinterpret_cast<PGM_P>(str2));
}

#if defined(ESP8266)

inline bool is_PGM_P(const void *ptr) {
    //return (const uintptr_t)ptr >= 0x40000000U;
    return (const uintptr_t)ptr >> 30;
}

inline bool is_aligned_PGM_P(const void * ptr)
{
    return (((const uintptr_t)ptr & 3) == 0);
}

inline bool is_not_PGM_P_or_aligned(const void * ptr)
{
    auto tmp = (const uintptr_t)ptr;
    return ((tmp >> 30) == 0) || ((tmp & 3) == 0);
}


#else


inline bool is_PGM_P(const void * ptr)
{
    return true;
}

inline bool is_aligned_PGM_P(const void * ptr)
{
    return (((const uintptr_t)ptr & 3) == 0);
}

inline bool is_not_PGM_P_or_aligned(const void * ptr)
{
    return is_aligned_PGM_P(ptr);
}

#endif

#if defined(ESP8266) || defined(ESP32)


PGM_P strchr_P(PGM_P str, int c);
PGM_P strrchr_P(PGM_P str, int c);
char *strdup_P(PGM_P src);

#else

inline PGM_P strchr_P(PGM_P src, int c) {
    return strchr(src, c);
}

inline PGM_P strrchr_P(PGM_P src, int c) {
    return strrchr(src, c);
}

inline char *strdup_P(PGM_P src) {
    return strdup(src);
}


#endif

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


// creates an object on the stack and returns a reference to it
//
// form.createHtml(&stack_reference<PrintArgsPrintWrapper>(Serial));

template<class T>
class stack_reference {
public:
    stack_reference(T &&object) : _object(std::move(object)) {
    }
    T &get() {
        return _object;
    }
    T &operator&() {
        return _object;
    }
private:
    T _object;
};

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
        return concat((const char *)str, size);
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

template<class G, class C>
String implode(G glue, const C &pieces, uint32_t max = (uint32_t)~0) {
    String tmp;
    if (max-- && pieces.begin() != pieces.end()) {
        auto iterator = pieces.begin();
        tmp += *iterator;
        while(max-- && ++iterator != pieces.end()) {
            tmp += glue;
            tmp += *iterator;
        }
    }
    return tmp;
}

template<class G, class C, class CB>
String implode_cb(G glue, const C &pieces, CB toString, uint32_t max = (uint32_t)~0) {
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

namespace xtra_containers {

    template <class T>
    void remove_duplicates(T &v)
    {
        auto end = v.end();
        for (auto it = v.begin(); it != end; ++it) {
            end = std::remove(it + 1, end, *it);
        }
        v.erase(end, v.end());
    }

};

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

#if _MSC_VER

#define IS_TIME_VALID(time) (time > 946684800UL)

#elif 1

typedef char end_of_time_t_2038[(time_t)(1UL<<31)==INT32_MIN ? 1 : -1];
#define IS_TIME_VALID(time) (((time_t)time < -1) || time > 946684800L)

#else

typedef char end_of_time_t_2106[(time_t)(1UL<<31)==(INT32_MAX+1UL) ? 1 : -1];
#define IS_TIME_VALID(time) (time > 946684800UL)

#endif

size_t strftime_P(char *buf, size_t size, PGM_P format, const struct tm *tm);

// seconds since boot
uint32_t getSystemUptime();

// milliseconds
uint64_t getSystemUptimeMillis();

// use signed char to get an integer
template<typename T>
String enumToString(T value) {
    return String(static_cast<typename std::underlying_type<T>::type>(value));
}

// auto address = convertToIPAddress("192.168.0.1");
// if (address.isSet()) { //we can use the address }
IPAddress convertToIPAddress(const char *hostname);

inline IPAddress convertToIPAddress(const String &hostname) {
    return convertToIPAddress(hostname.c_str());
}

#define __PP_CAT(a, b)                  __PP_CAT_I(a, b)
#define __PP_CAT_I(a, b)                __PP_CAT_II(~, a ## b)
#define __PP_CAT_II(p, res)             res
#define __UNIQUE_NAME(base)             __PP_CAT(__PP_CAT(__PP_CAT(base, __COUNTER__), _), __LINE__)


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

extern "C" const char SPGM_null[] PROGMEM;
extern "C" const char SPGM_0x_08x[] PROGMEM;

class SafeStringWrapper {
public:
    constexpr SafeStringWrapper() : _str(SPGM(null)) {}
    constexpr SafeStringWrapper(const char *str) : _str(str ? str : SPGM(null)) {}
    constexpr SafeStringWrapper(const __FlashStringHelper *str) : _str(str ? (PGM_P)str : SPGM(null)) {}
    constexpr const char *c_str() const {
        return _str;
    }
public:
    const char *_str;
};

inline const String __safeCString(const IPAddress &addr) {
    return addr.toString();
}

inline const String __safeCString(const void *ptr) {
    char buf[16];
    snprintf_P(buf, sizeof(buf), SPGM(0x_08x), ptr);
    return buf;
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

constexpr uint32_t kCreateIPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return a | (b << 8U) | (c << 16U) | (d << 24U);
}

inline uint32_t createIPv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return a | (b << 8U) | (c << 16U) | (d << 24U);
}

#define CREATE_ENUM_BITFIELD_SIZE(name, size, enum_type, underlying_type, underlying_type_name) \
    underlying_type name: size; \
    static constexpr size_t kBitCountFor_##name = size; \
    static void set_enum_##name(Type &obj, enum_type value) { \
        obj.name = static_cast<underlying_type>(value); \
    } \
    static enum_type get_enum_##name(const Type &obj) { \
        return static_cast<enum_type>(obj.name); \
    } \
    static void set_##underlying_type_name##_##name(Type &obj, underlying_type value) { \
        obj.name = value; \
    } \
    static underlying_type get_##underlying_type_name##_##name(const Type &obj) { \
        return obj.name; \
    } \
    static underlying_type cast_##underlying_type_name##_##name(enum_type value) { \
        return static_cast<underlying_type>(value); \
    } \
    static enum_type cast_enum_##name(underlying_type value) { \
        return static_cast<enum_type>(value); \
    }

#define CREATE_BITFIELD_TYPE(name, size, type, prefix) \
    type name: size; \
    static constexpr size_t kBitCountFor_##name = size; \
    static void set_##prefix##_##name(Type &obj, type value) { \
        obj.name = value; \
    } \
    static type get_##prefix##_##name(const Type &obj) { \
        return obj.name; \
    }

// Type must be defined inside the struct/class
// using Type = MyStructure_t;

// requires last value MAX to be defined: enum class uint8_t { NONE, VAL1, VAL2, MAX };
#define CREATE_ENUM_BITFIELD(name, enum_type)               CREATE_ENUM_BITFIELD_SIZE(name, kNumBitsRequired((int)enum_type::MAX - 1), enum_type, std::underlying_type<enum_type>::type, int)
#define CREATE_ENUM_N_BITFIELD(name, size, enum_type)       CREATE_ENUM_BITFIELD_SIZE(name, size, enum_type, std::underlying_type<enum_type>::type, int)

#define CREATE_BOOL_BITFIELD(name)                          CREATE_BITFIELD_TYPE(name, 1, bool, bit)

#define CREATE_UINT8_BITFIELD(name, size)                   CREATE_BITFIELD_TYPE(name, size, uint8_t, bits)
#define CREATE_UINT16_BITFIELD(name, size)                  CREATE_BITFIELD_TYPE(name, size, uint16_t, bits)
#define CREATE_UINT32_BITFIELD(name, size)                  CREATE_BITFIELD_TYPE(name, size, uint32_t, bits)

namespace std {

    template<typename Ta, typename Tb, typename Tr = typename std::common_type<typename std::make_unsigned<Ta>::type, typename std::make_unsigned<Tb>::type>::type>
    constexpr Tr max_unsigned(Ta a, Tb b) {
        return max(static_cast<Tr>(b), static_cast<Tr>(b));
    }

    template<typename Ta, typename Tb, typename Tr = typename std::common_type<typename std::make_signed<Ta>::type, typename std::make_signed<Tb>::type>::type>
    constexpr Tr max_signed(Ta a, Tb b) {
        return max(static_cast<Tr>(b), static_cast<Tr>(b));
    }

    template<typename Ta, typename Tb, typename Tr = typename std::common_type<typename std::make_unsigned<Ta>::type, typename std::make_unsigned<Tb>::type>::type>
    constexpr Tr min_unsigned(Ta a, Tb b) {
        return min(static_cast<Tr>(b), static_cast<Tr>(b));
    }

    template<typename Ta, typename Tb, typename Tr = typename std::common_type<typename std::make_signed<Ta>::type, typename std::make_signed<Tb>::type>::type>
    constexpr Tr min_signed(Ta a, Tb b) {
        return min(static_cast<Tr>(b), static_cast<Tr>(b));
    }

    template<typename Ta, typename Tb, typename Tc, typename Tr = typename std::common_type<typename std::make_unsigned<Ta>::type, typename std::make_unsigned<Tb>::type, typename std::make_unsigned<Tc>::type>::type>
    constexpr Tr clamp_unsigned(Ta v, Tb lo, Tc hi)
    {
        return (static_cast<Tr>(v) < static_cast<Tr>(lo)) ? static_cast<Tr>(lo) : (static_cast<Tr>(hi) < static_cast<Tr>(v)) ? static_cast<Tr>(hi) : static_cast<Tr>(v);
    }

    template<typename Ta, typename Tb, typename Tc, typename Tr = typename std::common_type<typename std::make_signed<Ta>::type, typename std::make_signed<Tb>::type, typename std::make_signed<Tc>::type>::type>
    constexpr Tr clamp_signed(Ta v, Tb lo, Tc hi)
    {
        return (static_cast<Tr>(v) < static_cast<Tr>(lo)) ? static_cast<Tr>(lo) : (static_cast<Tr>(hi) < static_cast<Tr>(v)) ? static_cast<Tr>(hi) : static_cast<Tr>(v);
    }



    // std::vector<std:unique_ptr<Test>> _timers;
    // Test *timer;
    // auto iterator = std::find_if(_timers.begin(), _timers.end(), std::compare_unique_ptr(timer));

    template <class Ta>
    class compare_unique_ptr_function : public unary_function<Ta, bool>
    {
    protected:
        Ta *_ptr;
    public:
        explicit compare_unique_ptr_function(Ta *ptr) : _ptr(ptr) {}
        bool operator() (const unique_ptr<Ta> &obj) const {
            return obj.get() == _ptr;
        }
    };

    template <class Ta>
    static inline compare_unique_ptr_function<Ta> compare_unique_ptr(Ta *ptr) {
        return compare_unique_ptr_function<Ta>(ptr);
    }

}
