/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

template<class T>
struct TypeName {
    static const __FlashStringHelper* name() { return nullptr; } \
    static const __FlashStringHelper* sname() { return nullptr; } \
    static const __FlashStringHelper* format() { return nullptr; } \
    static const char* name_cstr() { return nullptr; }
};

#define ENABLE_TYPENAME(T, SNAME, FORMAT) \
template<> \
struct TypeName<T> { \
    static const __FlashStringHelper* name() { return FPSTR(PSTR(#T)); } \
    static const __FlashStringHelper* sname() { return FPSTR(PSTR(#SNAME)); } \
    static const __FlashStringHelper* format() { return FPSTR(PSTR(FORMAT)); } \
    static const char* name_cstr() { return #T; } \
};

ENABLE_TYPENAME(char *, char *, "%s");
ENABLE_TYPENAME(const char *, const char *, "%s");
ENABLE_TYPENAME(void *, void *, "%p");
ENABLE_TYPENAME(const void *, const void *, "%p");
ENABLE_TYPENAME(void, void, "%p");
ENABLE_TYPENAME(char, char, "%c");
ENABLE_TYPENAME(signed char, int8_t, "%d");
ENABLE_TYPENAME(unsigned char, uint8_t, "%u");
ENABLE_TYPENAME(short, int16_t, "%d");
ENABLE_TYPENAME(unsigned short, uint16_t, "%u");
ENABLE_TYPENAME(int, int32_t, "%d");
ENABLE_TYPENAME(unsigned int, uint32_t, "%u");
ENABLE_TYPENAME(long, long, "%ll");
ENABLE_TYPENAME(unsigned long, ulong, "%ull");
ENABLE_TYPENAME(long long, int64_t, "%ll");
ENABLE_TYPENAME(unsigned long long, uint64_t, "%ull");
ENABLE_TYPENAME(float, float, "%f");
ENABLE_TYPENAME(double, double, "%f");
ENABLE_TYPENAME(bool, bool, "%u");
ENABLE_TYPENAME(nullptr_t, nullptr, "%p");
