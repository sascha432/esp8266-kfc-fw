#pragma once

#include <stdint.h>
#include <strings.h>
#include <pgmspace.h>

class String;
class __FlashStringHelper;
#ifndef PGM_P
#define PGM_P const char *
#endif

inline size_t String_replace(String &str, int from, int to);
inline size_t String_replaceIgnoreCase(String &str, int from, int to);

size_t String_ltrim(String &str);
size_t String_ltrim(String &str, const char *chars);
size_t String_ltrim_P(String &str, PGM_P chars);
size_t String_ltrim(String &str, char chars);
inline size_t String_ltrim(String &str, const __FlashStringHelper *chars);

size_t String_rtrim(String &str);
size_t String_rtrim(String &str, const char *chars, size_t minLength = ~0);
size_t String_rtrim_P(String &str, PGM_P chars, size_t minLength = ~0);
size_t String_rtrim(String &str, char chars, size_t minLength = ~0);
inline size_t String_rtrim(String &str, const __FlashStringHelper *chars);
inline size_t String_rtrim(String &str, const __FlashStringHelper *chars, size_t minLength);

size_t String_trim(String &str);
size_t String_trim(String &str, const char *chars);
size_t String_trim_P(String &str, PGM_P chars);
size_t String_trim(String &str, char chars);
inline size_t String_trim(String &str, const __FlashStringHelper *chars);

bool String_equals(const String &str1, PGM_P str2);
bool String_equalsIgnoreCase(const String &str1, PGM_P str2);
bool String_equals(const String &str1, const __FlashStringHelper *str2);
bool String_equalsIgnoreCase(const String &str1, const __FlashStringHelper *str2);

bool String_startsWith(const String &str1, const __FlashStringHelper *str2);
bool String_startsWith(const String &str1, PGM_P str2);
bool String_endsWith(const String &str1, const __FlashStringHelper *str2);
bool String_endsWith(const String &str1, PGM_P str2);


/*

class String;
class __FlashStringHelper;

size_t String_rtrim(String &str);
size_t String_rtrim(String &str, const char *chars);
size_t String_rtrim_P(String &str, PGM_P chars);
inline size_t String_rtrim(String &str, const __FlashStringHelper *chars, size_t minLength = ~0);

size_t String_ltrim(String &str);
size_t String_ltrim(String &str, const char *chars);
size_t String_ltrim_P(String &str, PGM_P chars);
inline size_t String_ltrim(String &str, const __FlashStringHelper *chars);

size_t String_trim(String &str);
size_t String_trim(String &str, const char *chars);
inline size_t String_trim(String &str, const __FlashStringHelper *chars);



inline bool String_equals(const String &str1, PGM_P str2);
inline bool String_equalsIgnoreCase(const String &str1, PGM_P str2);
inline bool String_equals_P(const String &str1, PGM_P str2);
inline bool String_equalsIgnoreCase_P(const String &str1, PGM_P str2);
inline bool String_equals(const String &str1, const __FlashStringHelper *str2);
inline bool String_equalsIgnoreCase(const String &str1, const __FlashStringHelper *str2);
inline bool String_startsWith(const String &str1, const __FlashStringHelper *str2);
inline bool String_endsWith(const String &str1, const __FlashStringHelper *str2);

*/
