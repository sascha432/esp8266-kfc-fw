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
#define DEBUG_PRINT_ARGS                                DEBUG_STRING_DEDUPLICATOR
#endif

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#if 0
#define __PADBG_printf_prefix(fmt, ...)                  __debug_prefix(DEBUG_OUTPUT); DEBUG_OUTPUT.printf_P(PSTR(fmt), ##__VA_ARGS__)
#define __PADBG_printf(fmt, ...)                         DEBUG_OUTPUT.printf_P(PSTR(fmt), ##__VA_ARGS__)
#define __PADBG_println()                                DEBUG_OUTPUT.println()
#else
#include "debug_helper_disable.h"
#define __PADBG_printf_prefix(...)
#define __PADBG_printf(...)
#define __PADBG_println()
#endif

class PrintArgs;

namespace PrintArgsHelper {

    static constexpr size_t kMaximumPrintfArguments = 15;

    enum class FormatType : uint8_t {
        NONE = 0,
        VPRINTF = NONE,

        /// etc....
        MAX = kMaximumPrintfArguments + 1,

        //ENCODE_HTML_ATTRIBUTE,
        //ENCODE_HTML_ENTITIES,
        //ENCODE_JSON,
        //ESCAPE_JAVASCRIPT,
        SINGLE_CHAR,
        SINGLE_STRING,
        SINGLE_CRLF,
        REPEAT_FORMAT,
        HTML_OUTPUT_BEGIN,
        HTML_CLOSE_QUOTE_CLOSE_TAG = HTML_OUTPUT_BEGIN,
        HTML_CLOSE_TAG,
        HTML_CLOSE_DIV,
        HTML_CLOSE_DIV_FORM_GROUP = HTML_CLOSE_DIV,
        HTML_CLOSE_DIV_RANGE_SLIDER = HTML_CLOSE_DIV,
        HTML_CLOSE_DIV_FORM_DEPENDENCY_GROUP = HTML_CLOSE_DIV,
        HTML_CLOSE_DIV_INPUT_GROUP_TEXT = HTML_CLOSE_DIV,
        HTML_CLOSE_DIV_2X,
        HTML_CLOSE_DIV_INPUT_GROUP_APPEND_DIV_INPUT_GROUP = HTML_CLOSE_DIV_2X,
        HTML_CLOSE_DIV_CARD_BODY_DIV_COLLAPSE = HTML_CLOSE_DIV_2X,
        HTML_CLOSE_DIV_3X,
        HTML_CLOSE_DIV_INPUT_GROUP_APPEND_DIV_INPUT_GROUP_DIV_FORM_GROUP = HTML_CLOSE_DIV_3X,
        HTML_CLOSE_DIV_CARD_BODY_DIV_COLLAPSE_DIV_CARD = HTML_CLOSE_DIV_3X,
        HTML_CLOSE_SELECT,
        HTML_CLOSE_LABEL,
        HTML_CLOSE_LABEL_WITH_COLON,
        HTML_OPEN_DIV_FORM_GROUP,
        HTML_OPEN_DIV_INPUT_GROUP,
        HTML_OPEN_DIV_INPUT_GROUP_APPEND,
        HTML_OPEN_DIV_INPUT_GROUP_TEXT,
        HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY,
        HTML_CLOSE_GROUP_START_HR,
        HTML_OUTPUT_END = HTML_CLOSE_GROUP_START_HR,
        MASK_NO_FORMAT = 0x7f
    };

    const __FlashStringHelper *getFormatByType(FormatType type);

    class BufferContext;

    static_assert(PrintArgsHelper::kMaximumPrintfArguments + 1 >= (size_t)PrintArgsHelper::FormatType::MAX, "invalid size");

}

class PrintArgs
{
public:
    static constexpr size_t kMaximumPrintfArguments = PrintArgsHelper::kMaximumPrintfArguments;

    using PrintInterface = PrintArgs;
    using FormatType = PrintArgsHelper::FormatType;

    PrintArgs(const PrintArgs &print) = delete;
    PrintArgs &operator=(const PrintArgs &print) = delete;

    PrintArgs() :
        _bufferPtr(nullptr),
        _position(0),
        _strLength(0)
    {
    }

    // output
    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

    // input
    void vprintf_P(const char *format, const uintptr_t **args, size_t numArgs);
    void print(char data);
    void print(const char *str);
    void print(const __FlashStringHelper *fpstr);
    void println(const char *str);
    void println(const __FlashStringHelper *fpstr);

    template<typename _Ta>
    void vprintf_P(const char *format, _Ta **args, size_t numArgs) {
        vprintf_P(format, (const uintptr_t **)args, numArgs);
    }

    template<typename _Ta>
    void vprintf_P(_Ta **args, size_t numArgs) {
        vprintf_P(*(const char **)&args[0], (const uintptr_t **)&args[1], numArgs);
    }

    template <typename... Args>
    void printf_P(const char *format, const Args &... args) {
        if (_position == PrintObject) {
            reinterpret_cast<Print *>(_bufferPtr)->printf_P(format, std::forward<const Args &>(args)...);
        }
        else {
            __PADBG_printf_prefix("args=%u fmt='%0.32s' ", sizeof...(args), format);
            size_t size = sizeof(format) + sizeof(FormatType);
            _calc_size(size, args...);
            __PADBG_printf("calc_size=%u buf_len=%u ", size, _buffer.length());

            // resize buffer to fit in all arguments
            _buffer.reserve(_buffer.length() + size);
            // use a pointer to avoid buffer.write() overhead
            _bufferPtr = _buffer.end() + sizeof(FormatType);
            _copy_32bit((uintptr_t)format);

            _collect(args...);

            __LDBG_assert_printf(size == (_bufferPtr - _buffer.end()), "calc_size=%u does not match size=%u", size, _bufferPtr - _buffer.end());

            // store number of pointers copied
            *_buffer.end() = ((_bufferPtr - &_buffer.end()[1]) / sizeof(uintptr_t)) - 1;
            // update length
            _buffer.setLength(_buffer.length() + size);

            __PADBG_printf("buf_len=%u\n", _buffer.length());

            _bufferPtr = nullptr;
        }
    }

    template <typename... Args>
    void printf_P(FormatType type, const char *format, uint8_t numArgs, const Args &... args) {
        _buffer.write((uint8_t)FormatType::REPEAT_FORMAT);
        _buffer.write(numArgs);
        printf_P(format, std::forward<const Args &>(args)...);
    }

    template <typename... Args>
    void printf_P(const __FlashStringHelper *format, const Args &... args) {
        printf_P(RFPSTR(format), std::forward<const Args &>(args)...);
    }

    void printf_P(FormatType type);

private:
    int _snprintf_P(uint8_t *buffer, size_t size, uintptr_t **args, uint8_t numArgs);

    // copy helper for unaligned target address
    inline void _copy_32bit(uintptr_t arg) {
#if 1
        // better performance than memcpy or std::copy
        *_bufferPtr++ = ((uint8_t *)&arg)[0];
        *_bufferPtr++ = ((uint8_t *)&arg)[1];
        *_bufferPtr++ = ((uint8_t *)&arg)[2];
        *_bufferPtr++ = ((uint8_t *)&arg)[3];
#else
        std::copy_n((uint8_t *)&arg, sizeof(arg), _bufferPtr);
        _bufferPtr += sizeof(arg);
#endif
    }

    inline void _copy_64bit(uint64_t arg) {
        std::copy_n((uint8_t *)&arg, sizeof(arg), _bufferPtr);
        _bufferPtr += sizeof(arg);
    }

    // convert to 32bit signed int, basically its padding with 0xff
    inline void _copy(int8_t arg) {
        _copy(static_cast<int32_t>(arg));
    }

    // convert to 32bit signed int
    inline void _copy(int16_t arg) {
        _copy(static_cast<int32_t>(arg));
    }

    // convert float to double
    inline void _copy(float arg) {
        _copy(static_cast<double>(arg));
    }

    inline void _copy(double arg) {
        std::copy_n((uint8_t *)&arg, sizeof(arg), _bufferPtr);
        //memcpy(_bufferPtr, &arg, sizeof(arg));
        _bufferPtr += sizeof(arg);
    }

    //// unsigned types can be zero padded to 32bit
    //template <typename T, typename std::enable_if<sizeof(T) < sizeof(uintptr_t), int>::type = 0>
    //inline void _copy(T arg) {
    //    _copy_32bit(static_cast<uintptr_t>(arg));
    //}

    // 32 bit args
    template <typename T, typename std::enable_if<sizeof(T) <= sizeof(uintptr_t), int>::type = 0>
    inline void _copy(T arg) {
        __PADBG_printf("arg=uintptr_t value=%08x ", (uintptr_t)arg);
        _copy_32bit((uintptr_t)arg);
    }

    // 64bit args
    template <typename T, typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), int>::type = 0>
    inline void _copy(T arg) {
        __PADBG_printf("arg=uint64_t dword=%08x ", (uint32_t)(arg));
        _copy_64bit((uint64_t)arg);
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

    // count size of arguments
    void _calc_size(size_t &size) {}

    void _calc_size(size_t &size, float) {
        size += sizeof(double);
    }

    template <typename T, typename std::enable_if<(sizeof(T) <= sizeof(uintptr_t)), int>::type = 0>
    void _calc_size(size_t &size, const T &t) {
        size += sizeof(uintptr_t);
    }

    template <typename T, typename std::enable_if<(sizeof(T) > sizeof(uintptr_t) && sizeof(T) <= sizeof(uint64_t)), int>::type = 0>
    void _calc_size(size_t &size, const T &t) {
        size += sizeof(uint64_t);
    }

    template <typename T, typename... Args>
    void _calc_size(size_t &size, const T &t, const Args &... args) {
        _calc_size(size, t);
        _calc_size(size, args...);
    }

protected:
    friend PrintArgsHelper::BufferContext;

    void _initOutput();

    static const int PrintObject = -1;

    uint8_t *_bufferPtr;            // pointer to input buffer
    int _position;                  // position in output buffer
    Buffer _buffer;                 // input buffer
    int _strLength;                 // length of current string to be copied
};



#include "PrintArgs.hpp"

#include "debug_helper_disable.h"
