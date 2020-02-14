/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
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

// G/T be any of char, const __FlashStringHelper *, const char *, String
template<class G, class T>
String implode(G glue, std::vector<T> *pieces) {
    String tmp;
    if (pieces && !pieces->empty()) {
        auto iterator = pieces->begin();
        tmp += *iterator;
        while(++iterator != pieces->end()) {
            tmp += glue;
            tmp += *iterator;
        }
    }
    return tmp;
}

String implode(const __FlashStringHelper *glue, const char **pieces, int count);
String implode(const __FlashStringHelper *glue, String *pieces, int count);

void explode(char* str, const char *separator, std::vector<char*> &vector, const char *trim = nullptr);


String url_encode(const String &str);
String printable_string(const uint8_t *buffer, size_t length);

void append_slash(String &dir);
void remove_trailing_slash(String &dir);

String sys_get_temp_dir();
#if SPIFFS_TMP_FILES_TTL
File tmpfile(const String &dir, const String &prefix, long ttl = SPIFFS_TMP_FILES_TTL);
#else
File tmpfile(const String &dir, const String &prefix);
#endif

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

#define NIBBLE2HEX_LC                           ('a' - 10)
#define NIBBLE2HEX_UC                           ('A' - 10)

// 4bit 2 hex
char nibble2hex(uint8_t nibble, char hex_char = NIBBLE2HEX_LC);

// memcpy with the minimum size of both variables
#define MEMNCPY_S(dst, src)                     { memcpy((void *)dst, (void *)src, min(sizeof(src), sizeof(dst))); }

// bzero variant using sizeof(dst)
#define BNZERO_S(dst)                           memset(&dst, 0, sizeof(dst));

#define STRINGLIST_SEPARATOR                    ','
#define STRLS                                   ","

// find a string in a list of strings separated by a single characters
// -1 = not found, otherwise the number of the matching string
int16_t stringlist_find_P_P(PGM_P list, PGM_P find, char separator = STRINGLIST_SEPARATOR);

// multiple separators
int16_t stringlist_find_P_P(PGM_P list, PGM_P find, const char *separator);

// compare two PROGMEM strings
int strcmp_P_P(PGM_P str1, PGM_P str2);

// compare two PROGMEM strings case insensitive
int strcasecmp_P_P(PGM_P str1, PGM_P str2);

int strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2);

size_t String_rtrim(String &str);
size_t String_ltrim(String &str);
size_t String_trim(String &str);

size_t String_rtrim(String &str, const char *chars);
size_t String_ltrim(String &str, const char *chars);
size_t String_trim(String &str, const char *chars);

size_t String_rtrim_P(String &str, const char *chars);
size_t String_ltrim_P(String &str, const char *chars);
size_t String_trim_P(String &str, const char *chars);

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

bool __while(uint32_t time_in_ms, std::function<bool()> loop, uint16_t interval_in_ms, std::function<bool()> intervalLoop);
// call intervalLoop every interval_in_ms for time_in_ms
bool __while(uint32_t time_in_ms, uint16_t interval_in_ms, std::function<bool()> intervalLoop);
// call loop every millisecond for time_in_ms
bool __while(uint32_t time_in_ms, std::function<bool()> loop);

#if ESP8266

const char *strchr_P(const char *str, int c);

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
