/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#include <BufferStream.h>
#include <PrintString.h>
#include <list>
#include <StringDepulicator.h>

#ifndef DEBUG_PRINT_ARGS
#define DEBUG_PRINT_ARGS                    1
#endif

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
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

//     class CharPtrContainer {
//     public:
//         CharPtrContainer(const CharPtrContainer &) = delete;
//         CharPtrContainer operator=(const CharPtrContainer &) = delete;

//         CharPtrContainer(CharPtrContainer &&str) noexcept;
//         CharPtrContainer &operator=(CharPtrContainer &&str) noexcept;

//         CharPtrContainer(char *str, bool free);
//         CharPtrContainer(const __FlashStringHelper *str);

//         operator const char *() const;
//         const char *c_str() const;

//         bool operator==(const char *str) const;
//         bool operator!=(const char *str) const;
//         ~CharPtrContainer();

//     private:
//         char *_str;
// #if !defined(ESP8266)
//         bool _free : 1;
// #endif
//     };

    PrintArgs(const PrintArgs &print) = delete;

    PrintArgs();
    ~PrintArgs();

    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

    // no type conversion: float, double and uint64_t need 2 arguments
    // uint8_t, uint16_t need to be zero padded
    void vprintf_P(const char *format, uintptr_t **args, size_t numArgs);

    template <typename... Args>
    void printf_P(const char *format, const Args &... args) {
#if DEBUG_PRINT_ARGS
        _printfCalls++;
#endif
        if (_position == PrintObject) {
            reinterpret_cast<Print *>(_bufferPtr)->printf_P(format, std::forward<const Args &>(args)...);
        }
        else {
#if DEBUG_PRINT_ARGS
            _printfArgs += sizeof...(args);
#endif
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

private:
    int _snprintf_P(uint8_t *buffer, size_t size, void **args, uint8_t numArgs);

    //// single printf stack argument
    //void _store(const uintptr_t data);

    template<typename T>
    constexpr size_t __padded_size() {
        return (sizeof(T) + 3) & ~3U;
    }

    // float needs to be converted to double
    void _copy(float value);
    // signed integers need to be converted
    void _copy(int8_t value8);
    void _copy(int16_t value16);
    // string de-duplication
    void _copy(const __FlashStringHelper *fpstr);
    void _copy(const char *str);

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

public:
    StringDeduplicator &strings() {
        return _strings;
    }

protected:
    static const int PrintObject = -1;

    uint8_t *_bufferPtr;
    int _position;
private:
    Buffer _buffer;
    int _strLength;
    StringDeduplicator _strings;
#if DEBUG_PRINT_ARGS
    size_t _outputSize;
    size_t _printfCalls;
    size_t _printfArgs;
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
