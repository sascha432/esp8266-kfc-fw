/**
 * Author: sascha_lammers@gmx.de
 */

#include <time.h>
#include "PrintString.h"

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

void PrintString::printFormatted(const __FlashStringHelper *format)
{
    print(format);
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
    __DBG_validatePointerCheck(format, VP_HPS);
    char temp[128];
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
    __DBG_validatePointerCheck(format, VP_HPS);
    char temp[128];
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
    auto len = length();
    if (!reserve(len + 1)) {
        return 0;
    }
    auto ptr = wbuffer() + len++;
    *ptr++ = static_cast<char>(data);
    *ptr = 0;
    setLen(len);
    return 1;
}

size_t PrintString::write(const uint8_t *buf, size_t size)
{
    return _write(buf, size);
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

size_t PrintString::_print(const uint32_t *str)
{
    union {
        uint32_t dword;
        uint8_t bytes[4];
    } tmp;
    uint8_t n;
    auto len = length();
    auto oldLen = len;
    do {
        tmp.dword = *str++;
        if (tmp.bytes[0] == 0) {
            break;
        }
        else if (tmp.bytes[1] == 0) {
            n = 1;
        }
        else if (tmp.bytes[2] == 0) {
            n = 2;
        }
        else if (tmp.bytes[3] == 0) {
            n = 3;
        }
        else {
            n = 4;
        }
        if (!reserve(len + n)) {
            break;
        }
        memmove_P(wbuffer() + len, tmp.bytes, n); // use memmove in case wbuffer() isn't aligned
        len += n;
        setLen(len);
    } while (n == 4);
    wbuffer()[len] = 0;

    return len - oldLen;
}

size_t PrintString::_print(PGM_P ptr)
{
    auto len = length();
    if (!reserve(len + 4)) { // reserves at least 5 byte
        return 0;
    }
    uint8_t written = 0;
    uint8_t ch;
    auto dst = wbuffer() + len;
    // check if the string is aligned
    switch ((reinterpret_cast<const uint32_t>(ptr) & 0x03)) {
    case 1: // 3 bytes to copy
        ch = pgm_read_byte(ptr++);
        if (!ch) {
            return 0;
        }
        *dst++ = ch;
        written++;
    case 2: // 2 bytes to copy
        ch = pgm_read_byte(ptr++);
        if (!ch) {
            *dst = 0;
            setLen(len + 1);
            return written;
        }
        *dst++ = ch;
        written++;
    case 3: // 1 byte left
        ch = pgm_read_byte(ptr++);
        if (!ch) {
            *dst = 0;
            setLen(len + 2);
            return written;
        }
        *dst++ = ch;
        *dst = 0;
        written++;
        setLen(len + 3);
        return written + _print(reinterpret_cast<const uint32_t *>(ptr));
    }
    return _print(reinterpret_cast<const uint32_t *>(ptr));
}

size_t PrintString::_write_P(const uint8_t *buf, size_t size)
{
    __DBG_validatePointerCheck(buf, VP_HPS);
    auto len = length();
    if (size && !reserve(len + size)) {
        return 0;
    }
    auto endPtr = reinterpret_cast<uint8_t *>(memmove_P(wbuffer() + len, buf, size));

    // this isac dding the NUL byte
    // basically *endPtr = 0;
    uint8_t offset = reinterpret_cast<uint32_t>(endPtr) & 0x03;
    auto endPtr32 = reinterpret_cast<uint32_t *>(endPtr - offset);
    *endPtr32 &= ~(0xff << (offset << 3));

    setLen(len + size);
    return size;
}
