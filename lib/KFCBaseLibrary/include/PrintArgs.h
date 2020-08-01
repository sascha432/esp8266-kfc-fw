/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#include <BufferStream.h>
#include <PrintString.h>
#include <list>

#ifndef DEBUG_PRINT_ARGS
#define DEBUG_PRINT_ARGS                    0
#endif

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#define DEBUG_CHECK_DUPES                   1
#else
#include "debug_helper_disable.h"
#define DEBUG_CHECK_DUPES                   1
#endif

// adds streaming capabilities to Print objects
// supported method: printf_P
// all pointers passed to printf_P must be valid until PrintArgs gets destroyed

// use PrintArgsPrintWrapper(Print &output) to write to any Print object without streaming (pass through mode)

// #include <TypeName.h>

class PrintArgs
{
public:
    static constexpr size_t kMaximumPrintfArguments = 15;

    using PrintInterface = PrintArgs;

    class CharPtrContainer {
    public:
        //CharPtrContainer() : _str(nullptr), _free(false) {}

        CharPtrContainer(const CharPtrContainer &) = delete;
        CharPtrContainer operator=(const CharPtrContainer &) = delete;

        CharPtrContainer(CharPtrContainer &&str) noexcept {
            *this = std::move(str);
        }
        CharPtrContainer &operator=(CharPtrContainer &&str) noexcept  {
            _str = str._str;
            str._str = nullptr;
#if !defined(ESP8266)
            _free = str._free;
            str._free = false;
#endif
            return *this;
        }

#if defined(ESP8266)
        CharPtrContainer(char *str, bool) : _str(str) {}
        CharPtrContainer(const __FlashStringHelper *str) : _str(reinterpret_cast<char *>(const_cast<__FlashStringHelper *>(str))) {}
#else
        CharPtrContainer(char *str, bool free) : _str(str), _free(free) {}
        CharPtrContainer(const __FlashStringHelper *str) : _str(reinterpret_cast<char *>(const_cast<__FlashStringHelper *>(str))), _free(false) {}
#endif

        operator const char *() const {
            return _str;
        }

        bool operator==(const char *str) const {
            return strcmp_P_P(_str, str) == 0; // function is nullptr sdafe
        }
        bool operator!=(const char *str) const {
            return strcmp_P_P(_str, str);
        }
        ~CharPtrContainer() {
#if defined(ESP8266)
            // detect if string is allocated
            if (_str && !is_PGM_P(_str)) {
                free(_str);
            }
#else
            if (_free) {
                free(_str);
            }
#endif
        }

    private:
        char *_str;
#if !defined(ESP8266)
        bool _free : 1;
#endif
    };

    PrintArgs(const PrintArgs &print) = delete;

    PrintArgs() : _bufferPtr(nullptr), _position(0), _strLength(0) {
#if DEBUG_PRINT_ARGS
        _outputSize = 0;
#endif
    }
    ~PrintArgs() {
#if DEBUG_PRINT_ARGS
        __DBG_print("--attached---");
        for(const auto &str: _attached) {
            __DBG_print(str);
        }
        __DBG_print("--end---");
#endif
    }

    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

    // no type conversion: float, double and uint64_t need 2 arguments
    // uint8_t, uint16_t need to be zero padded
    void vprintf_P(const char *format, uintptr_t **args, size_t numArgs);

    template <typename... Args>
    void printf_P(const char *format, const Args &... args) {
        if (_position == PrintObject) {
            reinterpret_cast<Print *>(_bufferPtr)->printf_P(format, std::forward<const Args &>(args)...);
        }
        else {
            // add counter for arguments and store position in buffer
            size_t counterLen = _buffer.length();
            _buffer.write(0);

            _buffer.write(reinterpret_cast<const uint8_t *>(&format), sizeof(const char *));
            _collect(args...);

            // update argument counter
            _buffer[counterLen] = (uint8_t)((_buffer.length() - counterLen - (sizeof(const char *) + sizeof(uint8_t))) / sizeof(uint32_t));
        }
    }

    template <typename... Args>
    void printf_P(const __FlashStringHelper *format, const Args &... args) {
        printf_P(RFPSTR(format), std::forward<const Args &>(args)...);
    }

public:
    // returns nullptr if the string is not attach
    // otherwise it returns the string or a string with the same content
    const char *getAttachedString(const char *str) const {
        auto iterator = std::find(_attached.begin(), _attached.end(), str);
        if (iterator != _attached.end()) {
            return *iterator;
        }
        return nullptr;
    }

    // attach a temporary string to the output object
    const char *attachString(char *str, bool free) {
#if DEBUG_CHECK_DUPES
        auto iterator = std::find(_attached.begin(), _attached.end(), str);
        if (iterator != _attached.end()) {
            __DBG_printf("duplicate string='%s'", (const char *)*iterator);
        }
#endif
        _attached.emplace_back(str, free);
        return str;
    }

    const char *attachString(const __FlashStringHelper *str) {
        return attachString(reinterpret_cast<char *>(const_cast<__FlashStringHelper *>(str)), false);
    }

    const char *attachString(const String &str) {
        return attachString(const_cast<char *>(str.c_str()), false);
    }

private:
    int _snprintf_P(uint8_t *buffer, size_t size, void **args, uint8_t numArgs);

    //// single printf stack argument
    //void _store(const uintptr_t data);

    template<typename T>
    constexpr size_t __padded_size() {
        return (sizeof(T) + 3) & ~3U;
    }

    // float needs to be converted to double
    void _copy(float value) {
        _copy((double)value);
    }

    // signed integers need to be converted
    void _copy(int8_t value8) {
        int32_t value = value8;
        _buffer.write(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

    void _copy(int16_t value16) {
        int32_t value = value16;
        _buffer.write(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

     // just copy dwords and add padding if required
    template<typename T>
    void _copy(T arg) {
        //__DBG_printf("type=%s %p size=%u aligned=%u", constexpr_TypeName<T>::name(), &arg, sizeof(T), __padded_size<T>());
        _buffer.write(reinterpret_cast<const uint8_t *>(&arg), sizeof(T));
        // add padding
        switch (sizeof(T) % 4) {
        case 1:
            _buffer.write(0);
            _buffer.write(0);
        case 3:
            _buffer.write(0);
            break;
        default:
            break;
        }
    }

    // process arguments
    void _collect() {}

    template <typename T>
    void _collect(const T &t) {
        _copy(t);
    }

    template <typename T, typename... Args>
    void _collect(const T &t, const Args &... args) {
        _copy(t);
        _collect(args...);
    }

// public:
//     void dump(Print &output);

protected:
    static const int PrintObject = -1;

    std::vector<CharPtrContainer> _attached;
    uint8_t *_bufferPtr;
    int _position;
private:
    Buffer _buffer;
    int _strLength;
#if DEBUG_PRINT_ARGS
    int _outputSize;
#endif
};

class PrintArgsPrintWrapper : public PrintArgs
{
public:
    PrintArgsPrintWrapper(Print &output) {
        _bufferPtr = reinterpret_cast<uint8_t *>(&output);
        _position = PrintArgs::PrintObject;
    }
    PrintArgsPrintWrapper(const PrintArgsPrintWrapper &wrapper) {
        _bufferPtr = wrapper._bufferPtr;
        _position = wrapper._position;
    }
};

#include "debug_helper_disable.h"
