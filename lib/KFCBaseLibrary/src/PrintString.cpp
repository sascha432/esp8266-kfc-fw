/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintString.h"


PrintString::PrintString(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

PrintString::PrintString(const char *format, va_list arg)
{
    vprintf(format, arg);
}

PrintString::PrintString(const __FlashStringHelper *format, ...)
{
    va_list arg;
    va_start(arg, format);
    vprintf_P(RFPSTR(format), arg);
    va_end(arg);
}

PrintString::PrintString(const __FlashStringHelper *format, va_list arg)
{
    vprintf_P(RFPSTR(format), arg);
}

PrintString::PrintString(double n, int digits, bool trimTrailingZeros)
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

size_t PrintString::print(double n, int digits, bool trimTrailingZeros)
{
    if (trimTrailingZeros) {
        char buf[64];
        snprintf_P(buf, sizeof(buf), PSTR("%.*f"), digits, n);
        auto endPtr = strchr(buf, '.');
        if (endPtr) {
            endPtr += 2;
            auto ptr = endPtr;
            while(*ptr) {
                if (*ptr++ != '0') {
                    endPtr = ptr;
                }
            }
            return Print::write(buf, endPtr - buf);
        }
        else {
            return Print::write(buf, strlen(buf));
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
        if (reserve(length() + len)) {
            return vsnprintf(begin() + length(), len + 1, format, arg);
        }
        return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(temp), len);
}

size_t PrintString::vprintf_P(PGM_P format, va_list arg)
{
    char temp[64];
    size_t len = vsnprintf_P(temp, sizeof(temp), format, arg);
    if (len > sizeof(temp) - 1) {
        if (reserve(length() + len)) {
            return vsnprintf_P(begin() + length(), len + 1, format, arg);
        }
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
    if (!reserve(length() + size)) {
        return 0;
    }
#if WSTRING_HAVE_SETLEN
    reinterpret_cast<char *>(memmove(begin() + length(), buf, size))[size] = 0;
    setLen(length() + size);
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
    if (!reserve(length() + size)) {
        return 0;
    }
#if WSTRING_HAVE_SETLEN
    reinterpret_cast<char *>(memcpy_P(begin() + length(), buf, size))[size] = 0;
    setLen(length() + size);
#else
    size_t count = size;
    while (count--) {
        *this += (char)pgm_read_byte(buf++);
    }
#endif
    return size;
}
