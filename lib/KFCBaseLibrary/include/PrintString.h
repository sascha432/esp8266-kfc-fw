/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * String class with "Print" interface
 **/

#pragma once

#include <Arduino_compat.h>

class PrintString : public String, public Print {
public:
    PrintString() : String(), Print() {
    }
//    PrintString(const char *format, ...) __attribute__((format(printf, 2, 3))) : String(), Print() {
    PrintString(const char *format, ...) : String(), Print() {
        va_list arg;
        va_start(arg, format);
        vprintf(format, arg);
        va_end(arg);
    }
    PrintString(const char *format, va_list arg) : String(), Print() {
        vprintf(format, arg);
    }
    PrintString(const __FlashStringHelper *format, ...) : String(), Print() {
        va_list arg;
        va_start(arg, format);
        vprintf_P(reinterpret_cast<PGM_P>(format), arg);
        va_end(arg);
    }
    PrintString(const __FlashStringHelper *format, va_list arg) : String(), Print() {
        vprintf_P(reinterpret_cast<PGM_P>(format), arg);
    }

    size_t vprintf(const char *format, va_list arg) {
        char temp[64];
        // char* buffer = temp;
        size_t len = vsnprintf(temp, sizeof(temp), format, arg);
        if (len > sizeof(temp) - 1) {
            if (reserve(length() + len)) {
                return vsnprintf(begin() + length(), len + 1, format, arg);
            }
            return 0;
            // buffer = new char[len + 1];
            // if (!buffer) {
            //     return 0;
            // }
            // vsnprintf(buffer, len + 1, format, arg);
        }
        return write((const uint8_t *)temp, len);;
        // len = write((const uint8_t *)buffer, len);
        // if (buffer != temp) {
        //     delete[] buffer;
        // }
        // return len;
    }

    size_t vprintf_P(PGM_P format, va_list arg) {
        char temp[64];
        // char* buffer = temp;
        size_t len = vsnprintf_P(temp, sizeof(temp), format, arg);
        if (len > sizeof(temp) - 1) {
            if (reserve(length() + len)) {
                return vsnprintf_P(begin() + length(), len + 1, format, arg);
            }
            return 0;
            // buffer = new char[len + 1];
            // if (!buffer) {
            //     return 0;
            // }
            // vsnprintf_P(buffer, len + 1, format, arg);
        }
        return write((const uint8_t *)temp, len);;
        // len = write((const uint8_t *)buffer, len);
        // if (buffer != temp) {
        //     delete[] buffer;
        // }
        // return len;
    }

    virtual size_t write(uint8_t data) override {
        return concat((char)data) ? 1 : 0;
    }
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        return concat((const char *)buffer, size);
    }
};
