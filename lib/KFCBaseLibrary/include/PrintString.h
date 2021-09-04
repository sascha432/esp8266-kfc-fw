/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * String class with "Print" interface
 **/

#pragma once

#include <Arduino_compat.h>

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#ifndef WSTRING_HAVE_SETLEN
#    if ESP8266
#        define WSTRING_HAVE_SETLEN 1
#    elif defined(_MSC_VER)
#        define WSTRING_HAVE_SETLEN 0
#    else
#        define WSTRING_HAVE_SETLEN 1
#    endif
#endif

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

class PrintString : public String, public Print {
public:
    using Print::print;
    using Print::println;
    using String::operator+=;

    PrintString();
    PrintString(char ch) : String(ch) {}
    PrintString(const String &str);

    PrintString(const char *format, va_list arg);

    PrintString(const __FlashStringHelper *format, va_list arg);
    PrintString(double n, uint8_t digits, bool trimTrailingZeros);
    PrintString(const uint8_t* buffer, size_t len);
    PrintString(const __FlashBufferHelper *buffer, size_t len);


    // PrintString(const char *str)
    // PrintString(const char *format, ...) with at least one argument, otherwise it is not using printf but print

    template<typename ..._Args>
    PrintString(const char *format, _Args ...args) {
        printFormatted(format, args...);
    }
    void printFormatted(const char *format) {
        print(format);
    }
    template<typename _Ta, typename ..._Args>
    void printFormatted(const char *format, _Ta arg, _Args ...args) {
        Print::printf_P(format, arg, args...);
    }

    // PrintString(const __FlashStringHelper *format)
    // PrintString(const __FlashStringHelper *format, ...) with at least one argument, otherwise it is not using printf but print

    template<typename ..._Args>
    PrintString(const __FlashStringHelper *format, _Args ...args) {
        printFormatted(format, args...);
    }
    void printFormatted(const __FlashStringHelper *format);
    template<typename _Ta, typename ..._Args>
    void printFormatted(const __FlashStringHelper *format, _Ta arg, _Args ...args) {
        Print::printf_P(reinterpret_cast<PGM_P>(format), arg, args...);
    }

    size_t print(double n, uint8_t digits, bool trimTrailingZeros);
    size_t vprintf(const char *format, va_list arg);
    size_t vprintf_P(PGM_P format, va_list arg);
    size_t write_P(PGM_P buf, size_t size);
    size_t print(const __FlashStringHelper *str);
    size_t println(const __FlashStringHelper *str);

    size_t strftime_P(PGM_P format, struct tm *tm);
    size_t strftime(const char *format, struct tm *tm);
    size_t strftime(const __FlashStringHelper *format, struct tm *tm);

    size_t strftime_P(PGM_P format, time_t now);
    size_t strftime(const char *format, time_t now);
    size_t strftime(const __FlashStringHelper *format, time_t now);

    PrintString &operator+=(unsigned long long);
    PrintString &operator+=(long long);

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buf, size_t size) override;
    size_t write(const char *buf, size_t size);


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
    size_t _write(const uint8_t *buf, size_t size);
    size_t _write_P(const uint8_t *buf, size_t size);

    size_t _write(char ch);
    size_t _print(const uint32_t *str);
    size_t _print(PGM_P str);

    // returns old length() before increasing
    size_t _increaseLength(size_t newLen);
};


inline __attribute__((__always_inline__))
size_t PrintString::println(const __FlashStringHelper *str)
{
    auto before = length();
    concat(str);
    _write('\r');
    _write('\n');
    return length() - before;
}

inline __attribute__((__always_inline__))
size_t PrintString::write(const char *buf, size_t size)
{
    return _write(reinterpret_cast<const uint8_t *>(buf), size);
}

inline __attribute__((__always_inline__))
size_t PrintString::_write(const uint8_t *buf, size_t size)
{
    __DBG_validatePointerCheck(buf, VP_HS);
    auto len = length();
    if (size && !reserve(len + size)) {
        return 0;
    }
    reinterpret_cast<char *>(memmove(wbuffer() + len, buf, size))[size] = 0;
    setLen(len + size);
    return size;
}

inline __attribute__((__always_inline__))
size_t PrintString::_write(char ch)
{
    return write(static_cast<uint8_t>(ch));
}


inline __attribute__((__always_inline__))
size_t PrintString::write_P(PGM_P buf, size_t size)
{
    return _write_P(reinterpret_cast<const uint8_t *>(buf), size);
}

inline __attribute__((__always_inline__))
size_t PrintString::print(const __FlashStringHelper *str)
{
    __DBG_validatePointerCheck(str, VP_HPS);
    return Print::print(str);
}

inline __attribute__((__always_inline__))
PrintString::PrintString() : String(), Print()
{
}

inline __attribute__((__always_inline__))
PrintString::PrintString(const String &str) : String(str)
{
}

inline __attribute__((__always_inline__))
PrintString::PrintString(const char *format, va_list arg) : String(), Print()
{
    vprintf(format, arg);
}

inline __attribute__((__always_inline__))
PrintString::PrintString(const __FlashStringHelper *format, va_list arg) : String(), Print()
{
    vprintf_P(RFPSTR(format), arg);
}

inline __attribute__((__always_inline__))
PrintString::PrintString(double n, uint8_t digits, bool trimTrailingZeros) : String(), Print()
{
    print(n, digits, trimTrailingZeros);
}

inline __attribute__((__always_inline__))
PrintString::PrintString(const uint8_t *buffer, size_t len) : String(), Print()
{
    write(buffer, len);
}

inline __attribute__((__always_inline__))
PrintString::PrintString(const __FlashBufferHelper *buffer, size_t len) : String(), Print()
{
    write_P(reinterpret_cast<PGM_P>(buffer), len);
}

inline __attribute__((__always_inline__))
size_t PrintString::strftime(const char *format, struct tm *tm)
{
    char temp[128];
    ::strftime(temp, sizeof(temp), format, tm);
    return print(temp);
}

inline size_t PrintString::strftime(const __FlashStringHelper *format, tm *tm) {
    return strftime_P(reinterpret_cast<PGM_P>(format), tm);
}

inline __attribute__((__always_inline__))
size_t PrintString::strftime_P(PGM_P format, struct tm *tm)
{
    char temp[128];
    ::strftime_P(temp, sizeof(temp), format, tm);
    return print(temp);
}

inline __attribute__((__always_inline__))
size_t PrintString::strftime_P(PGM_P format, time_t now)
{
    return strftime_P(format, localtime(&now));
}

inline __attribute__((__always_inline__))
size_t PrintString::strftime(const char *format, time_t now)
{
    return strftime(format, localtime(&now));
}

inline __attribute__((__always_inline__))
size_t PrintString::strftime(const __FlashStringHelper *format, time_t now)
{
    return strftime(format, localtime(&now));
}

inline __attribute__((__always_inline__))
PrintString &PrintString::operator+=(unsigned long long value)
{
    print(value);
    return *this;
}

inline __attribute__((__always_inline__))
PrintString &PrintString::operator+=(long long value)
{
    print(value);
    return *this;
}

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif
