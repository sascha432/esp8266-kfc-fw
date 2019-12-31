/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <algorithm>
#include <array>
#include <functional>
#include <type_traits>
#include <utility>
#include <array_of.h>
#include <MillisTimer.h>
#include <crc16.h>

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

extern const String _sharedEmptyString;

#define STATIC_STRING_BUFFER_SIZE 33

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
//     //     if (!reserve(length() + alen)) {
//     //         invalidate();
//     //     } else {
//     //         setLen(length() + alen);
//     //         strncpy(begin() + length(), cstr, alen)[alen] = 0;
//     //     }
//     // }
// };

// #endif

// pretty format for bytes and unix time
String formatBytes(size_t bytes);
String formatTime(unsigned long seconds, bool days_if_not_zero = false);

String implode(const __FlashStringHelper *glue, const char **pieces, int count);
String implode(const __FlashStringHelper *glue, String *pieces, int count);
String url_encode(const String &str);
String printable_string(const uint8_t *buffer, size_t length);

String append_slash_copy(const String &dir);
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

// find a string in a list of strings separated by a single characters
// -1 = not found, otherwise the number of the matching string
int16_t stringlist_find_P_P(PGM_P list, PGM_P find, char separator = ',');

// multiple separators
int16_t stringlist_find_P_P(PGM_P list, PGM_P find, const char *separator);

// compare two PROGMEM strings
int strcmp_P_P(PGM_P str1, PGM_P str2);

// compare two PROGMEM strings case insensitive
int strcasecmp_P_P(PGM_P str1, PGM_P str2);

int strcmp_end_P(const char *str1, size_t len1, PGM_P str2, size_t len2);

// compare functions that do not create a String object of "str2"
bool String_equals(const String &str1, PGM_P str2);
bool String_equalsIgnoreCase(const String &str1, PGM_P str2);
bool String_startsWith(const String &str1, PGM_P str2);
bool String_endsWith(const String &str1, PGM_P str2);
bool String_endsWith(const String &str1, char ch);

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

// parse arguments into tokens
// return value == maxArgs might indicate that arguments have been truncated
// str='command=t1,t2,t3,"t3-1,t3-2", "t4-1""t4-2" , "t5-1\"t5-2\t5-3\\t5-4"'
// 0='t1'
// 1='t2'
// 2='t3'
// 3='t3-1,t3-2'
// 4='t4-1"t4-2'
// 5='t5-1"t5-2\t5-3\t5-4'
uint8_t tokenizer(char *str, char **args, uint8_t maxArgs, bool hasCommand = true);

#define repeat_first_iteration \
    (__repeat_iteration == 0)
#define repeat_last_iteration \
    (__repeat_iteration == (__repeat_count - 1))
#define repeat(count, ...) { \
        const auto  __repeat_count = count; \
        for(auto __repeat_iteration = 0; __repeat_iteration < __repeat_count; ++__repeat_iteration) { \
            __VA_ARGS__; \
        } \
    }
