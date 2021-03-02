/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * String class with "Print" interface
 **/

#pragma once

#include <Arduino_compat.h>
#include "int64_to_string.h"

#ifndef WSTRING_HAVE_SETLEN
#if ESP8266
#define WSTRING_HAVE_SETLEN                 1
#elif defined(_MSC_VER)
#define WSTRING_HAVE_SETLEN                 0
#else
#define WSTRING_HAVE_SETLEN                 1
#endif
#endif
class PrintString : public String, public Print {
public:
    PrintString();
    PrintString(const String &str);

    PrintString(const char *format, ...);
    PrintString(const char *format, va_list arg);
    PrintString(const __FlashStringHelper *format, ...);
    PrintString(const __FlashStringHelper *format, va_list arg);
    PrintString(double n, uint8_t digits, bool trimTrailingZeros);
    PrintString(const uint8_t* buffer, size_t len);
    PrintString(const __FlashBufferHelper *buffer, size_t len);

    size_t print(double n, uint8_t digits, bool trimTrailingZeros);
    size_t vprintf(const char *format, va_list arg);
    size_t vprintf_P(PGM_P format, va_list arg);
    size_t write_P(PGM_P buf, size_t size);

    size_t print(uint64_t value);
    size_t print(int64_t value);
    using Print::print;

    size_t strftime_P(PGM_P format, struct tm *tm);
    size_t strftime(const char *format, struct tm *tm);
    size_t strftime(const __FlashStringHelper *format, struct tm *tm) {
        return strftime_P(reinterpret_cast<PGM_P>(format), tm);
    }

    size_t strftime_P(PGM_P format, time_t now) {
        return strftime_P(format, localtime(&now));
    }
    size_t strftime(const char *format, time_t now) {
        return strftime(format, localtime(&now));
    }
    size_t strftime(const __FlashStringHelper *format, time_t now) {
        return strftime(format, localtime(&now));
    }

    PrintString &operator+=(uint64_t);
    PrintString &operator+=(int64_t);
    using String::operator+=;

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t* buf, size_t size) override;
    size_t write(const char *buf, size_t size) {
        return write(reinterpret_cast<const uint8_t *>(buf), size);
    }


#if defined(ESP8266)
    static constexpr size_t getSSOSIZE() {
        return String::SSOSIZE;
    }
#else
    static constexpr size_t getSSOSIZE() {
        return 0xffff;
    }
#endif

private:
    // returns old length() before increasing
    size_t _increaseLength(size_t newLen);
};
