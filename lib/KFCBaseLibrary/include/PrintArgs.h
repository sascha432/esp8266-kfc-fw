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
        // vprintf < FormatType::MAX
        NONE = 0,
        VPRINTF = NONE,

        /// etc....
        MAX = kMaximumPrintfArguments + 1,

        // custom printf functions < FormatType::MAX_FUNCS

        //ENCODE_HTML_ATTRIBUTE,
        //ENCODE_HTML_ENTITIES,
        //ENCODE_JSON,
        //ESCAPE_JAVASCRIPT,
        SINGLE_CHAR,
        SINGLE_STRING,
        SINGLE_CRLF,
        VPRINTF_REPEAT,
        VPRINTF_TYPE,
        MAX_FUNCS,

        // single strings >= FormatType::STRINGS_BEGIN && <= STRINGS_END
        STRINGS_BEGIN = MAX_FUNCS,
        HTML_CLOSE_QUOTE_CLOSE_TAG = STRINGS_BEGIN,
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
        HTML_CLOSE_TEXTAREA,
        HTML_LABEL_FOR,
        HTML_LABEL_FOR_COLON,
        HTML_ATTRIBUTE_STR_STR,
        HTML_ATTRIBUTE_STR_INT,
        HTML_OPEN_SELECT,
        HTML_OPEN_HIDDEN_SELECT,
        HTML_OPEN_TEXTAREA,
        HTML_OPEN_TEXT_INPUT,
        HTML_OPEN_NUMBER_INPUT,
        HTML_OPEN_GROUP_START_CARD,
        HTML_OPEN_GROUP_START_CARD_COLLAPSE_HIDE,
        HTML_OPEN_GROUP_START_CARD_COLLAPSE_SHOW,
        HTML_OPEN_DIV_FORM_GROUP,
        HTML_OPEN_DIV_INPUT_GROUP,
        HTML_OPEN_DIV_INPUT_GROUP_APPEND,
        HTML_OPEN_DIV_INPUT_GROUP_TEXT,
        HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY,
        HTML_OPTION_NUM_KEY_SELECTED,
        HTML_OPTION_STR_KEY_SELECTED,
        HTML_OPTION_NUM_KEY,
        HTML_OPTION_STR_KEY,
        HTML_CLOSE_GROUP_START_HR,
        STRINGS_END = HTML_CLOSE_GROUP_START_HR,

        // last bit is set for VPRINTF_TYPE
        VPRINTF_TYPE_MASK = 0x80
    };

    const __FlashStringHelper *getFormatByType(FormatType type);

    class BufferContext;

    static_assert(PrintArgsHelper::kMaximumPrintfArguments + 1 >= (size_t)PrintArgsHelper::FormatType::MAX, "invalid size");
    static_assert(sizeof(FormatType) == 1, "size does not match");
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
#if DEBUG_PRINT_ARGS
        ,_stats({})
#endif
    {
    }

public:
    // output
    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

public:
    // input
    void vprintf_P(const uintptr_t format, const uintptr_t *args, size_t numArgs);

    void print(const char *str);
    void print(const __FlashStringHelper *fpstr);
    void println(const char *str);
    void println(const __FlashStringHelper *fpstr);

    template<typename _Ta>
    void vprintf_P(const char *format, _Ta *args, size_t numArgs) {
        vprintf_P((const uintptr_t)format, (const uintptr_t *)args, numArgs);
    }

    template<typename _Ta>
    void vprintf_P(_Ta *args, size_t numArgs) {
        vprintf_P((const char *)args[0], (const uintptr_t *)&args[1], numArgs);
    }

public:
    // default printf_P
    template <typename _Ta, typename... Args>
    void printf_P(_Ta format, const Args &... args) {
        vprintf_va_list(nullptr, 0, (const char *)format, args...);
    }

    // repeat format x repeatCount
    // using numArgs / repeatCount for each repetition
    template <typename... Args>
    void printf_P_repeat_fmt(const char *format, uint8_t repeatNum, const Args &... args) {
        uint8_t data[] = { static_cast<uint8_t>(FormatType::VPRINTF_REPEAT), repeatNum };
        vprintf_va_list(data, sizeof(data), format, args...);
    }

    // use FormatType as format string
    template <typename... Args>
    void printf_P(FormatType type, const Args &... args) {
        __LDBG_assert_printf((type >= FormatType::STRINGS_BEGIN && type <= FormatType::STRINGS_END), "type=%u invalid", type);
        uint8_t data[] = { static_cast<uint8_t>(static_cast<uint8_t>(type) | static_cast<uint8_t>(FormatType::VPRINTF_TYPE_MASK)) };
        vprintf_va_list(data, sizeof(data), args...);
    }

    // print FormatType as raw string
    // format specifiers are not replaced
    void printf_P(FormatType type) {
        __LDBG_assert_printf((type >= FormatType::STRINGS_BEGIN && type <= FormatType::STRINGS_END), "type=%u invalid", type);
        _buffer.write(static_cast<uint8_t>(type));
    }

protected:
    // create va_list style list of arguments inside buffer
    // arguments are stored as 32bit values except float, double and uint64_t, those are 64bit
    // count starts with 0, representing 1 argument, etc...
    template <typename... Args>
    void vprintf_va_list(uint8_t *data, size_t dataLen, const Args &... args)
    {
        size_t size = sizeof(FormatType) + _calc_size(args...);
        __PADBG_printf("size=%u data=%u buf=%u args=%u ", size, dataLen, _buffer.length(), sizeof...(args));

        // resize buffer to fit in all arguments and data
        _buffer.reserve(_buffer.length() + size + dataLen);
        if (dataLen) {
            _buffer.write(data, dataLen);
        }

        // use a pointer to avoid buffer.write() overhead
        _bufferPtr = _buffer.end() + sizeof(FormatType);
        _collect(args...);

        __LDBG_assert_printf(size == (size_t)(_bufferPtr - _buffer.end()), "calc_size=%u does not match size=%u", (int)size, (int)(_bufferPtr - _buffer.end()));

        // store number of pointers copied
        *_buffer.end() = ((_bufferPtr - &_buffer.end()[1]) / sizeof(uintptr_t)) - 1;
        // update length
        _buffer.setLength(_buffer.length() + size);

        __PADBG_printf("buf_len=%u arg_count=%u\n", _buffer.length(), *(_buffer.end() - size) + 1);

        _bufferPtr = nullptr;
    }

private:
    int _snprintf_P(uint8_t *buffer, size_t size, const uintptr_t *args, uint8_t numArgs);

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

    // convert char[] to pointer
    inline void _copy(const char arg[]) {
        _copy((uintptr_t)&arg[0]);
    }

    // convert uint8_t[] to pointer
    inline void _copy(const uint8_t arg[]) {
        _copy((uintptr_t)&arg[0]);
    }

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
    inline size_t _calc_size() {
        return 0;
    }

    inline size_t _calc_size(float) {
        return sizeof(double);
    }

    // convert char[] to pointer
    inline size_t _calc_size(const char arg[]) {
        return sizeof(uintptr_t);
    }

    // convert uint8_t[] to pointer
    inline size_t _calc_size(const uint8_t arg[]) {
        return sizeof(uintptr_t);
    }

    template <typename T, typename std::enable_if<(sizeof(T) <= sizeof(uintptr_t)), int>::type = 0>
    size_t _calc_size(const T &t) {
        return sizeof(uintptr_t);
    }

    template <typename T, typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), int>::type = 0>
    size_t _calc_size(const T &t) {
        return sizeof(uint64_t);
    }

    template <typename T, typename... Args>
    size_t _calc_size(const T &t, const Args &... args) {
        return _calc_size(t) + _calc_size(args...);
    }

protected:
    friend PrintArgsHelper::BufferContext;

    void _initOutput();

    //static const int PrintObject = -1;

    uint8_t *_bufferPtr;            // pointer to input buffer in fillbuffer() / output iterator in printf_P()
    Buffer _buffer;                 // input buffer
    int _position;                  // stores the start offset of vprintf() / copyString() if the operation has to be repeated cause the buffer was full
    int _strLength;                 // length of current string to be copied
#if DEBUG_PRINT_ARGS
    struct {
        size_t outputSize;
        struct {
            uint16_t fillBuffer;
            uint16_t copyString;
            uint16_t vprintf;
            uint16_t numArgs[kMaximumPrintfArguments + 1];
        } calls;
        struct {
            uint16_t copyString;
            uint16_t vprintf;
        } maxLen;
        struct {
            uint16_t copyString;
            uint16_t vprintf;
        } repeat;
    } _stats;
#endif
};

#include "PrintArgs.hpp"

#include "debug_helper_disable.h"
