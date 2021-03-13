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


#include "cores/esp8266/WString.h"

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

#if CUSTOM_STRING_CLASS

class String : public std::string {
public:
    String() = default;
    
    String(bool value);
    String(int16_t value);
    String(uint16_t value);
    String(int64_t value);
    String(uint64_t value);
    String(float value, unsigned char decimals = 2);
    String(double value, unsigned char decimals = 2);
    String(const char ch);
    String(const char *str);
    String(String &&str) noexcept;
    String(const String &str);
    String(const __FlashStringHelper *str);
    ~String();

    explicit String(int value, unsigned char base = 10);
    explicit String(unsigned int value, unsigned char base = 10);
    explicit String(long value, unsigned char base = 10);
    explicit String(unsigned long value, unsigned char base = 10);

    int indexOf(const String &find, int index = 0) const;
    int indexOf(char ch, int index = 0) const;

    char *begin();
    String substring(size_t from) const;
    String substring(size_t from, size_t to) const;
    void remove(int index, size_t count = -1);
    void trim(void);
    void toLowerCase(void);
    void toUpperCase(void);
    void replace(char find, char replace);
    void replace(String fromStr, String toStr);

    unsigned char concat(const char *cstr, unsigned int length);
    unsigned char concat(char c);

    long toInt() const;
    float toFloat() const;
    double toDouble() const;

    unsigned char reserve(size_t size);

    bool equals(const String &value) const;
    bool equals(const char *value) const;
    bool equalsIgnoreCase(const String &value) const noexcept;
    bool equalsIgnoreCase(const char *value) const noexcept;

    bool startsWith(const String str) const;
    bool endsWith(const String str) const;

    char charAt(size_t idx) const;
    void setCharAt(size_t idx, char ch) {
        at(idx) = ch;
    }

    String &operator =(String &&str) noexcept;
    String &operator =(const String &str);

    String operator + (const String &str);
    String operator + (const char *str);
    String operator + (const char ch);
    String operator + (const __FlashStringHelper *str);

    String &operator += (const String &str);
    String &operator += (char *str);
    String &operator += (const char *str);
    String &operator += (const char ch);
    String &operator += (const __FlashStringHelper *str);

    operator const char *() const {
        return c_str();
    }

    LPWStr LPWStr();
    std::wstring w_str();
};

namespace esp8266 {
    using String = String;
}

#else

using WString = esp8266::String;

class String : public esp8266::String {
public:
    using esp8266::String::String;

    using esp8266::String::capacity;

    String substring(unsigned int left) const {
        return substring(left, length());
    }
    String substring(unsigned int left, unsigned int right) const {
        if (left > right) {
            unsigned int temp = right;
            right = left;
            left = temp;
        }
        String out;
        if (left >= len())
            return out;
        if (right > len())
            right = len();
        char temp = buffer()[right];  // save the replaced character
        wbuffer()[right] = '\0';
        out = wbuffer() + left;  // pointer arithmetic
        wbuffer()[right] = temp;  //restore character
        return out;
    }

    //inline unsigned int capacity() const {
    //    return String::capacity();
    //}
};

#endif