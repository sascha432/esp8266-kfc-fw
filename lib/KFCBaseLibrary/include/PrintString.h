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
    PrintString(const String &str) : String(str), Print() {
    }
    PrintString(const char *format, ...) : PrintString() {
        va_list arg;
        va_start(arg, format);
        vprintf(format, arg);
        va_end(arg);
    }
    PrintString(const char *format, va_list arg) : PrintString() {
        vprintf(format, arg);
    }
    PrintString(const __FlashStringHelper *format, ...) : PrintString() {
        va_list arg;
        va_start(arg, format);
        vprintf_P(reinterpret_cast<PGM_P>(format), arg);
        va_end(arg);
    }
    PrintString(const __FlashStringHelper *format, va_list arg) : PrintString() {
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
        return write(reinterpret_cast<const uint8_t *>(temp), len);
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
        return write(reinterpret_cast<const uint8_t *>(temp), len);
        // len = write((const uint8_t *)buffer, len);
        // if (buffer != temp) {
        //     delete[] buffer;
        // }
        // return len;
    }

    virtual size_t write(uint8_t data) override {
        *this += (char)data;
        return 1;
        //return concat((char)data) ? 1 : 0;
    }
    virtual size_t write(const uint8_t *buffer, size_t size) override {
        size_t count = size;
        if (!reserve(length() + size)) {
            return 0;
        }
        const char *ptr = reinterpret_cast<const char *>(buffer);
        while(count--) {
            *this += *ptr++;
        }
        return size;

        // concat keeps crashing for an unknown reason (when the string is being free'd)
        // sample code, which works perfectly using the + operator and loop above
        // PrintString command;
        // command.write(data, len);

        //return concat((const char *)buffer, size) ? size : 0;
    }
};
