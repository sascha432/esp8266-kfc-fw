/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <xstring>
#include "Print.h"

class String;

class LPWStr {
public:
    LPWStr();
    LPWStr(const String& str);
    ~LPWStr();

    LPWSTR lpw_str(const String &str);
    LPWSTR lpw_str();

private:
    LPWSTR _str;
};

class String : public std::string {
public:
    String() : std::string() {
    }

    String(bool value) : String() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    String(int16_t value) : String() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    String(uint16_t value) : String() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        assign(buf);
    }
    //String(int32_t value) : String() {
    //    char buf[16];
    //    snprintf(buf, sizeof(buf), "%d", value);
    //    assign(buf);
    //}
    //String(uint32_t value) : String() {
    //    char buf[16];
    //    snprintf(buf, sizeof(buf), "%u", value);
    //    assign(buf);
    //}
    String(int64_t value) : String() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", value);
        assign(buf);
    }
    String(uint64_t value) : String() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", value);
        assign(buf);
    }
    //String(long value) : String() {
    //    char buf[16];
    //    snprintf(buf, sizeof(buf), "%ld", value);
    //    assign(buf);
    //}
    //String(unsigned long value) : String() {
    //    char buf[16];
    //    snprintf(buf, sizeof(buf), "%lu", value);
    //    assign(buf);
    //}
    String(float value, unsigned char decimals = 2) : String() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, value);
        assign(buf);
    }
    String(double value, unsigned char decimals = 2) : String() {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, value);
        assign(buf);
    }
    String(const char ch) : String() {
        assign(1, ch);
    }
    String(const char *str) : String() {
        assign(str);
    }
    String(const String& str) : String() {
        assign(str);
    }
    String(const __FlashStringHelper* str) : String() {
        assign(reinterpret_cast<const char*>(str));
    }

    explicit String(int value, unsigned char base = 10) : String() {
        char buf[65]; // max 64bit + NUL
        _itoa(value, buf, base);
        *this = buf;
    }
    explicit String(unsigned int value, unsigned char base = 10) : String() {
        char buf[65]; // max 64bit + NUL
        _itoa(value, buf, base);
        *this = buf;
    }
    explicit String(long value, unsigned char base = 10) : String() {
        char buf[65]; // max 64bit + NUL
        _ltoa(value, buf, base);
        *this = buf;
    }
    explicit String(unsigned long value, unsigned char base = 10) : String() {
        char buf[65]; // max 64bit + NUL
        _ltoa(value, buf, base);
        *this = buf;
    }

    int indexOf(const String &find, int index = 0) const {
        return find_first_of(find, index);
    }
    int indexOf(char ch, int index = 0) const {
        return find_first_of(ch, index);
    }
    String substring(size_t from) const {
        return String(substr(from).c_str());
    }
    String substring(size_t from, size_t to) const {
        return String(substr(from, to).c_str());
    }
    long toInt() const {
        return atol(c_str());
    }
    float toFloat() const {
        return (float)atof(c_str());
    }
    unsigned char reserve(size_t size) {
        std::string::reserve(size);
        return (capacity() >= size);
    }
    bool equals(const String &value) const {
        return *this == value;
    }
    bool equals(const char *value) const {
        return strcmp(c_str(), value) == 0;
    }
    bool equalsIgnoreCase(const String &value) const {
        return _stricmp(c_str(), value.c_str()) == 0;
    }
    bool equalsIgnoreCase(const char *value) const {
        return _stricmp(c_str(), value) == 0;
    }
    void remove(int index, size_t count = -1) {
        if (index < 0) {
            index = length() + index;
        }
        if (count == -1 || index + count >= length()) {
            assign(substr(0, index));
        } else {
            assign(substr(0, index) + substr(index + count));
        }
    }
    char *begin() {
        return (char *)c_str();
    }
    void trim(void) {
        char *buffer = begin();
        size_t len = length();
        if (!buffer || len == 0) return;
        char *begin = buffer;
        while (isspace(*begin)) begin++;
        char *end = buffer + len - 1;
        while (isspace(*end) && end >= begin) end--;
        len = end + 1 - begin;
        if (begin > buffer) memmove(buffer, begin, len);
        buffer[len] = 0;
        assign(buffer);
    }
    void toLowerCase(void) {
        if (length()) {
            for (char *p = begin(); *p; p++) {
                *p = tolower(*p);
            }
        }
    }
    void toUpperCase(void) {
        if (length()) {
            for (char *p = begin(); *p; p++) {
                *p = toupper(*p);
            }
        }
    }
    void replace(char find, char replace) {
        if (length()) {
        }
        for (char *p = begin(); *p; p++) {
            if (*p == find) *p = replace;
        }
    }
    void replace(String fromStr, String toStr) {
        size_t pos = find(fromStr);
        while (pos != String::npos) {
            std::string::replace(pos, fromStr.length(), toStr);
            pos = find(fromStr, pos + toStr.length());
        }
    }

    unsigned char concat(const char *cstr, unsigned int length) {
        if (!cstr) {
            return 0;
        }
        if (length == 0) {
            return 1;
        }
        this->append(cstr, length);
        return length;
    }

    unsigned char concat(char c) {
        this->append(1, c);
        return 1;
    }

    bool startsWith(const String str) const {
        if (length() < str.length()) {
            return false;
        }
        return (strncmp(c_str(), str.c_str(), str.length()) == 0);
    }

    bool endsWith(const String str) const {
        if (length() < str.length()) {
            return false;
        }
        return strcmp(c_str() + length() - str.length(), str.c_str()) == 0;
    }

    char charAt(size_t idx) const {
        return at(idx);
    }

    String & operator =(const String &str) {
        assign(str);
        return *this;
    }

    String& operator + (const String& str) {
        concat(str.c_str(), str.length());
        return (*this);
    }
    String& operator += (const String& str) {
        concat(str.c_str(), str.length());
        return (*this);
    }
    String & operator += (char *str) {
        concat(str, strlen(str));
        return (*this);
    }
    String& operator + (const char* str) {
        concat(str, strlen(str));
        return (*this);
    }
    String& operator += (const char* str) {
        concat(str, strlen(str));
        return (*this);
    }
    String& operator + (const char ch) {
        concat(ch);
        return (*this);
    }
    String& operator += (const char ch) {
        concat(ch);
        return (*this);
    }
    String& operator + (const __FlashStringHelper* str) {
        const char* ptr = reinterpret_cast<const char*>(str);
        concat(ptr, strlen(ptr));
        return (*this);
    }
    String& operator += (const __FlashStringHelper* str) {
        const char* ptr = reinterpret_cast<const char*>(str);
        concat(ptr, strlen(ptr));
        return (*this);
    }

    LPWStr LPWStr() {
        return ::LPWStr(*this);
    }

    std::wstring w_str() {
        return std::wstring(std::string::begin(), std::string::end());
    }
};
