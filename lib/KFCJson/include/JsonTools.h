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
        Utf8Buffer() : _length(0), _buf{} {}

        // use this function to add more data
        // -127 = the last sequence was invalid. the current byte can be dropped or the
        // invalid sequence added. the previous bytes are stored in _buf. clear() must
        // be called to continue using the buffer
        // -126 invalid unicode value. treat like -255...
        // -2 = the character does not require encoding
        // -1 = more data required
        // >=0 = unicode value
        inline __attribute__((__always_inline__))
        int32_t feed(char ch) {
            return feed(static_cast<uint8_t>(ch));
        }

        inline __attribute__((__always_inline__))
        int32_t feed(int ch) {
            return feed(static_cast<uint8_t>(ch));
        }

        int32_t feed(uint8_t ch) {
            // not encoding required
            if (_length == 0 && ch < 0x80) {
                return static_cast<int32_t>(ErrorType::NO_ENCODING_REQUIRED);
            }
            // buffer full / invalid sequence
            if (static_cast<uint8_t>(_length) >= sizeof(_buf)) {
                return static_cast<int32_t>(ErrorType::INVALID_SEQUENCE);
            }
            _buf[_length++] = ch;
            auto result = utf8_length();
            if (result == _length) {
            // if the end marker is found check if we have collect all the bytes already
            //if ((ch & 0xc0) == 0x80) 
                switch(_length) {
                case 4: { // 4 byte sequence
                        uint32_t tmp = ((_buf[0] & 0b111) << 18) | ((_buf[1] & 0b111111) << 12) | ((_buf[2] & 0b111111) << 6) | (ch & 0b111111);
                        if (tmp <= 0x10ffff) {
                            return tmp;
                        }
                        // invalid unicode
                        return static_cast<int32_t>(ErrorType::INVALID_UNICODE_SYMBOL);
                    }
                case 3: // 3 byte sequence
                    return ((_buf[0] & 0b1111) << 12) | ((_buf[1] & 0b111111) << 6) | (ch & 0b111111);
                case 2: // 2 byte sequence
                    return ((_buf[0] & 0b11111) << 6) | (ch & 0b111111);
                case 0:
                    break;
                default:
                    return static_cast<int32_t>(ErrorType::INVALID_SEQUENCE);
                }
            }
            return static_cast<int32_t>(ErrorType::MORE_DATA_REQUIRED);
        }

        inline __attribute__((__always_inline__))
        void clear() {
            _length = 0;
        }

        inline __attribute__((__always_inline__))
        uint8_t length() const {
            return _length;
        }

        uint8_t unicodeLength(int32_t unicode) const {
           if (static_cast<uint32_t>(unicode) < 0x10000) {
                return 6;
            }
            else if (static_cast<uint32_t>(unicode) <= 0x10ffff) {
                return 12;
            }
            return 0;
        }

        size_t printTo(Print &print, int32_t unicode) {
            if (static_cast<uint32_t>(unicode) < 0x10000) {
                return print.printf_P(PSTR("\\u%04x"), static_cast<uint32_t>(unicode));
            }
            else if (static_cast<uint32_t>(unicode) <= 0x10ffff) {
                return print.printf_P(PSTR("\\u%04x\\u%04x"), 0, 0); // TODO encode to 2x UTF16
            }
            return 0;
        }

    private:
        int8_t utf8_length() const {
            uint8_t code = (*_buf >> 3);
            if (code == 0b11110) {
                return 4;   // 21bit
            }
            else if (code == 0b11100) {
                return 3;   // 16bit
            }
            else if (code == 0b11000) {
                return 2;   // 11bit
            }
            return -127;
        }

    private:
        int8_t _length;
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
    size_t print(uint64_t value) {
        // no encoding required
        return print_string(_printString, value);
    }

    inline __attribute__((__always_inline__))
    size_t print(int64_t value) {
        // no encoding required
        return print_string(_printString, value);
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
