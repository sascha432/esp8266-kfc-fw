/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#if defined(HAVE_FLASH_STRING_GENERATOR) && HAVE_FLASH_STRING_GENERATOR
#include "../src/generated/FlashStringGeneratorAuto.h"
#endif
#if _MSC_VER
#include "../../../src/generated/FlashStringGeneratorAuto.h"
#endif
#include <algorithm>
#include <array>
#include <vector>
#include <functional>
#include <type_traits>
#include <utility>
#include <array_of.h>
#include <MillisTimer.h>
#include <crc16.h>
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
String formatTime(unsigned long seconds, bool days_if_not_zero = false);

String url_encode(const String &str);
void printable_string(Print &output, const uint8_t *buffer, size_t length, size_t maxLength = 0, const char *extra = nullptr);
String printable_string(const uint8_t *buffer, size_t length, size_t maxLength = 0);
inline String printable_string(const char *buffer, size_t length, size_t maxLength = 0) {
    return printable_string(reinterpret_cast<const uint8_t *>(buffer), length, maxLength);
}

void append_slash(String &dir);
void remove_trailing_slash(String &dir);

String sys_get_temp_dir();
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

#define MAC2STRING_LEN                          18

// not using printf
String mac2String(const uint8_t *mac, char separator = ':');

// size >= 18 or >= 13 if separator = 0
// returns "null" if mac is nullptr
const char *mac2char_s(char *dst, size_t size, const uint8_t *mac, char separator = ':');

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

#if ESP8266 || ESP32

const char *strchr_P(PGM_P str, int c);
const char *strrchr_P(PGM_P str, int c);

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

namespace split {

    typedef enum {
        NONE =          0x00,
        EMPTY =         0x01,       // add empty strings
        TRIM =          0x02,       // trim strings
        _PROGMEM =      0x04,
    } SplitFlagsType;

    typedef void(*callback)(const char *str, size_t len, void *data, int flags);

    void vector_callback(const char *sptr, size_t len, void *ptr, int flags);
    void split(const char *str, char sep, callback fun, void *data, int flags = SplitFlagsType::EMPTY, uint16_t limit = UINT16_MAX);
    void split_P(PGM_P str, char sep, callback fun, void *data, int flags = SplitFlagsType::EMPTY, uint16_t limit = UINT16_MAX);
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

inline void explode(const char *str, const char ch, StringVector &container, uint16_t limit = UINT16_MAX) {
    split::split(str, ch, split::vector_callback, &container, split::SplitFlagsType::EMPTY, limit);
}

inline void explode_P(const char *str, const char ch, StringVector &container, uint16_t limit = UINT16_MAX) {
    split::split_P(str, ch, split::vector_callback, &container, split::SplitFlagsType::EMPTY, limit);
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
        if ((uint32_t)addr <= 0x40000000) {
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
