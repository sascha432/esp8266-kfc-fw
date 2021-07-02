/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "JsonString.h"

#define DEBUG_COLLECT_STRING_ENABLE     0

#if DEBUG_COLLECT_STRING_ENABLE

#include <vector>
#include <type_traits>


extern void __debug_json_string_dump(Stream &out);
extern int __debug_json_string_skip(const String &str);
extern int __debug_json_string_add(const String &str);
extern int __debug_json_string_add(const char *str);
extern int __debug_json_string_add(const __FlashStringHelper *);

template <typename T>
int __debug_json_string_add(T str) {
    return 0;
}

extern std::vector<String> __debug_json_string_list;

#define DEBUG_COLLECT_STRING(str)               __debug_json_string_add(str)

#else

#define DEBUG_COLLECT_STRING(str)

#endif

class JsonTools {
public:
    // class to buffer and convert bytes (utf8) to unicode
    class Utf8Buffer {
    public:
        enum class ErrorType : int32_t {
            INVALID_SEQUENCE = -127,
            INVALID_UNICODE_SYMBOL =  -126,
            NO_ENCODING_REQUIRED = -2,
            MORE_DATA_REQUIRED = -1,
        };

    public:
        Utf8Buffer() : _counter(0), _buf{} {}

        // use this function to add more data
        // INVALID_UNICODE_SYMBOL/INVALID_SEQUENCE the current sequence is invalid and skipped
        // to retrieve the data, store counter() before calling the method. it returns the number
        // of bytes stored in _buf that will be discarded
        // feed more data if MORE_DATA_REQUIRED is returned
        // >=0 = codepoint / UTF32
        inline __attribute__((__always_inline__))
        int32_t feed(char ch) {
            return feed(static_cast<uint8_t>(ch));
        }

        inline __attribute__((__always_inline__))
        int32_t feed(int ch) {
            return feed(static_cast<uint8_t>(ch));
        }

        int32_t feed(uint8_t ch);

        inline __attribute__((__always_inline__))
        void clear() {
            _counter = 0;
        }

        // current length of the utf8 buffer or 0 for emtpy
        inline __attribute__((__always_inline__))
        uint8_t counter() {
            return _counter;
        }

        // total length of the current sequence (2, 3 or 4) or 0 for none
        inline uint8_t length() const {
            return _counter ? (utf8_length(*_buf) + 1) : 0;
        }

        // encoded length of the codepoint
        uint8_t unicodeLength(uint32_t codepoint) const {
           if (codepoint < 0x10000) {
                return 6;
            }
            else if (codepoint <= 0x10ffff) {
                return 12;
            }
            return 0;
        }

        // convert UTF32 to JSON UTF16
        size_t printTo(Print &print, uint32_t codepoint) {
            if (codepoint < 0x10000) {
                return print.printf_P(PSTR("\\u%04x"), codepoint);
            }
            else if (codepoint <= 0x10ffff) {
                return print.printf_P(PSTR("\\u%04x\\u%04x"), static_cast<uint16_t>(0xd7c0 + (codepoint >> 10)), static_cast<uint16_t>(0xdc00 + (codepoint & 0x3ff)));
            }
            return 0;
        }

    private:
        inline uint8_t utf8_length(uint8_t code) const {
            if ((code & 0b11110000) == 0b11110000) {
                return 3;       // 21bit
            }
            else if ((code & 0b111100000) == 0b11100000) {
                return 2;       // 16bit
            }
            else if ((code & 0b111000000) == 0b11000000) {
                return 1;       // 11bit
            }
            return 0xf;
        }

        inline bool isNotData(uint8_t code) const {
            return ((code & 0b110000000) != 0b10000000);
        }

    private:
        uint8_t _counter;
        uint8_t _buf[3];
    };

    using ErrorType = JsonTools::Utf8Buffer::ErrorType;

    static const __FlashStringHelper *boolToString(bool value);
    static size_t lengthEscaped(PGM_P value, size_t length, Utf8Buffer *buffer = nullptr);
    static size_t printToEscaped(Print &output, PGM_P value, size_t length, Utf8Buffer *buffer = nullptr);

    inline __attribute__((__always_inline__))
    static size_t lengthEscaped(const JsonString &value, Utf8Buffer *buffer = nullptr) {
        return lengthEscaped(value.getPtr(), value.length(), buffer);
    }

    inline __attribute__((__always_inline__))
    static size_t lengthEscaped(const String &value, Utf8Buffer *buffer = nullptr) {
        return lengthEscaped(value.c_str(), value.length(), buffer);
    }

    inline __attribute__((__always_inline__))
    static size_t lengthEscaped(const __FlashStringHelper *value, Utf8Buffer *buffer = nullptr) {
        return lengthEscaped(RFPSTR(value), strlen_P(RFPSTR(value)), buffer);
    }

    inline __attribute__((__always_inline__))
    static size_t printToEscaped(Print &output, const String &value, Utf8Buffer *buffer = nullptr) {
        return printToEscaped(output, value.c_str(), value.length(), buffer);
    }

    inline __attribute__((__always_inline__))
    static size_t printToEscaped(Print &output, const __FlashStringHelper *value, Utf8Buffer *buffer = nullptr) {
        return printToEscaped(output, RFPSTR(value), strlen_P(RFPSTR(value)), buffer);
    }

    inline __attribute__((__always_inline__))
    static size_t printToEscaped(Print &output, const JsonString &value, Utf8Buffer *buffer = nullptr) {
        return printToEscaped(output, value.getPtr(), value.length(), buffer);
    }

    inline __attribute__((__always_inline__))
    static bool escape(char ch) {
        switch (ch) {
        case '\b':
        case '\f':
        case '\t':
        case '\r':
        case '\n':
        case '\\':
        case '\"':
            return true;
        }
        return false;
    }

    inline __attribute__((__always_inline__))
    static size_t printEscaped(Print &output, char ch, Utf8Buffer *buffer = nullptr) {
        if (!output.write('\\')) {
            return 0;
        }
        switch (ch) {
        case '\b':
            return output.write('b') + 1;
        case '\f':
            return output.write('f') + 1;
        case '\t':
            return output.write('t') + 1;
        case '\r':
            return output.write('r') + 1;
        case '\n':
            return output.write('n') + 1;
        case '\\':
            return output.write('\\') + 1;
        case '"':
            return output.write('"') + 1;
        }
        return 1;
    }
};


// PrintString with JSON value encoding
class JsonPrintString : public PrintString {
public:
    using PrintString::PrintString;

    virtual size_t write(uint8_t data) override {
        return JsonTools::printToEscaped(*this, (const char *)&data, 1);
    }

    virtual size_t write(const uint8_t *buf, size_t size) override {
        return write(reinterpret_cast<const char *>(buf), size);
    }

    size_t write(const char *buf, size_t size) {
        auto requiredLen = JsonTools::lengthEscaped(buf, size);
        if (!reserve(length() + requiredLen)) {
            return 0;
        }
        return JsonTools::printToEscaped(*this, buf, size);
    }

};

// A print wrapper for PrintString
// the wrapper does not require any casting, allocations or move operations and provides all methods of PrintString with
// JSON encoding and UTF8 support with just 12 bytes on the stack
class JsonPrintStringWrapper : public Print {
public:
    using Print::print;
    using Utf8Buffer = JsonTools::Utf8Buffer;

    JsonPrintStringWrapper(PrintString &printString) :
        _printString(printString),
        _buffer()
    {
    }

    inline __attribute__((__always_inline__))
    void clear() {
        _printString.clear();
        _buffer.clear();
    }

    inline __attribute__((__always_inline__))
    size_t print(double n, uint8_t digits, bool trimTrailingZeros) {
        // no encoding required
        return _printString.print(n, digits, trimTrailingZeros);
    }

    inline __attribute__((__always_inline__))
    size_t vprintf(const char *format, va_list arg) {
        return vprintf(format, arg);
    }

    inline __attribute__((__always_inline__))
    size_t vprintf_P(PGM_P format, va_list arg) {
        return printf_P(format, arg);
    }

    inline __attribute__((__always_inline__))
        size_t strftime_P(PGM_P format, struct tm *tm) {
        char temp[64];
        ::strftime_P(temp, sizeof(temp), format, tm);
        return print(temp);
    }

    inline __attribute__((__always_inline__))
        size_t strftime(const char *format, struct tm *tm) {
        char temp[64];
        ::strftime(temp, sizeof(temp), format, tm);
        return print(temp);
    }

    inline __attribute__((__always_inline__))
    size_t strftime(const __FlashStringHelper *format, struct tm *tm) {
        return strftime_P(reinterpret_cast<PGM_P>(format), tm);
    }

    inline __attribute__((__always_inline__))
    size_t strftime_P(PGM_P format, time_t now) {
        return strftime_P(format, localtime(&now));
    }

    inline __attribute__((__always_inline__))
    size_t strftime(const char *format, time_t now) {
        return strftime(format, localtime(&now));
    }

    inline __attribute__((__always_inline__))
    size_t strftime(const __FlashStringHelper *format, time_t now) {
        return strftime(format, localtime(&now));
    }

    virtual size_t write(uint8_t data) override {
        return JsonTools::printToEscaped(_printString, (const char *)&data, 1, &_buffer);
    }

    virtual size_t write(const uint8_t *buf, size_t size) override {
        return write(reinterpret_cast<const char *>(buf), size);
    }

    size_t write(const char *buf, size_t size) {
        auto requiredLen = JsonTools::lengthEscaped(buf, size);
        _printString.reserve(_printString.length() + requiredLen);
        return JsonTools::printToEscaped(_printString, buf, size, &_buffer);
    }

    inline __attribute__((__always_inline__))
    size_t length() const {
        return _printString.length();
    }

private:
    PrintString &_printString;
    Utf8Buffer _buffer;
};
