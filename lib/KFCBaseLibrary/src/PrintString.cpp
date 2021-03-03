/**
 * Author: sascha_lammers@gmx.de
 */

#include <time.h>
#include "PrintString.h"

PrintString::PrintString()
{
}

PrintString::PrintString(const String &str) : String(str)
{
}

PrintString::PrintString(const char *format, va_list arg)
{
    vprintf(format, arg);
}

PrintString::PrintString(const __FlashStringHelper *format, va_list arg)
{
    vprintf_P(RFPSTR(format), arg);
}

PrintString::PrintString(double n, uint8_t digits, bool trimTrailingZeros)
{
    print(n, digits, trimTrailingZeros);
}

PrintString::PrintString(const uint8_t* buffer, size_t len)
{
    write(buffer, len);
}

PrintString::PrintString(const __FlashBufferHelper *buffer, size_t len)
{
    write_P(reinterpret_cast<PGM_P>(buffer), len);
}

size_t PrintString::print(double n, uint8_t digits, bool trimTrailingZeros)
{
    if (trimTrailingZeros) {
        char buf[std::numeric_limits<double>::digits + 1];
        auto len = snprintf_P(buf, sizeof(buf), PSTR("%.*f"), digits, n);
        if (digits == 0) {
            return write(reinterpret_cast<const uint8_t *>(buf), len);
        }
        else {
            auto endPtr = strchr(buf, '.');
            if (!endPtr) {
                return 0;
            }
            endPtr += 2;
            auto ptr = endPtr;
            while(*ptr) {
                if (*ptr++ != '0') {
                    endPtr = ptr;
                }
            }
            return write(reinterpret_cast<const uint8_t *>(buf), endPtr - buf);
        }
    }
    else {
        return Print::print(n, digits);
    }
}

size_t PrintString::vprintf(const char *format, va_list arg)
{
    char temp[64];
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    if (len > sizeof(temp) - 1) {
        auto strLen = length();
#if WSTRING_HAVE_SETLEN
        if (reserve(strLen + len)) {
            len = vsnprintf(wbuffer() + strLen, len + 1, format, arg);
            setLen(strLen + len);
            return len;
        }
#else
        if (reserve(strLen + len)) {
            return vsnprintf(begin() + _increaseLength(strLen + len), len + 1, format, arg);
        }
#endif
        return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(temp), len);
}

size_t PrintString::vprintf_P(PGM_P format, va_list arg)
{
    char temp[64];
    size_t len = vsnprintf_P(temp, sizeof(temp), format, arg);
    if (len > sizeof(temp) - 1) {
        auto strLen = length();
#if WSTRING_HAVE_SETLEN
        if (reserve(strLen + len)) {
            len = vsnprintf_P(wbuffer() + strLen, len + 1, format, arg);
            setLen(strLen + len);
            return len;
        }
#else
        if (reserve(strLen + len)) {
            return vsnprintf_P(begin() + _increaseLength(strLen + len), len + 1, format, arg);
        }
#endif
        return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(temp), len);
}

size_t PrintString::write(uint8_t data)
{
    *this += (char)data;
    return 1;
}

size_t PrintString::write(const uint8_t* buf, size_t size)
{
    auto len = length();
    if (!reserve(len + size)) {
        return 0;
    }
#if WSTRING_HAVE_SETLEN
    reinterpret_cast<char *>(memmove(wbuffer() + len, buf, size))[size] = 0;
    setLen(len + size);
#elif 1
    memmove(begin() + _increaseLength(len + size), buf, size);
#else
    const char* ptr = reinterpret_cast<const char*>(buf);
    size_t count = size;
    while (count--) {
        *this += *ptr++;
    }
#endif
    return size;
}

size_t PrintString::write_P(PGM_P buf, size_t size)
{
    auto len = length();
    if (!reserve(len + size)) {
        return 0;
    }
#if WSTRING_HAVE_SETLEN
    reinterpret_cast<char *>(memcpy_P(wbuffer() + len, buf, size))[size] = 0;
    setLen(len + size);
#elif 1
    memcpy_P(begin() + _increaseLength(len + size), buf, size);
#else
    size_t count = size;
    while (count--) {
        *this += (char)pgm_read_byte(buf++);
    }
#endif
    return size;
}

size_t PrintString::print(uint64_t value)
{
    return concat_to_string(*this, value);
}

size_t PrintString::print(int64_t value)
{
    return concat_to_string(*this, value);
}

size_t PrintString::strftime(const char *format, struct tm *tm)
{
    char temp[64];
    ::strftime(temp, sizeof(temp), format, tm);
    return print(temp);
}

size_t PrintString::strftime_P(PGM_P format, struct tm *tm)
{
    char temp[64];
    ::strftime_P(temp, sizeof(temp), format, tm);
    return print(temp);
}


PrintString &PrintString::operator+=(uint64_t value)
{
    print(value);
    return *this;
}

PrintString &PrintString::operator+=(int64_t value)
{
    print(value);
    return *this;
}

size_t PrintString::_increaseLength(size_t newLen)
{
    auto len = length();
    if (newLen <= len) {
        return len;
    }
    newLen -= len;
    while(newLen--) {
        *this += (char)0;
    }
    return len;
}
