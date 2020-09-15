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
#define DEBUG_PRINT_ARGS_PRINT(fmt, ...)                _debugPrint(PSTR(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_ARGS_PRINT(...)
#endif


#define DEBUG_PRINT_MSC_VER  0


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

    enum class FormatType : uint8_t {
        NONE = 0,
        /// etc....
        MAX = kMaximumPrintfArguments + 1,
        ENCODE_HTML_ATTRIBUTE,
        ENCODE_HTML_ENTITIES,
        ENCODE_JSON,
        ESCAPE_JAVASCRIPT,
        SINGLE_CHAR,
        SINGLE_STRING,
        REPEAT_FORMAT,
        HTML_CLOSE_QUOTE_CLOSE_TAG,
        HTML_CLOSE_TAG,
        HTML_CLOSE_DIV,
        HTML_CLOSE_DIV_2X,
        HTML_CLOSE_DIV_3X,
        HTML_CLOSE_SPAN,
        HTML_CLOSE_SELECT,
        HTML_CLOSE_LABEL,
        HTML_CLOSE_LABEL_WITH_COLON,
        HTML_OPEN_DIV_FORM_GROUP,
        HTML_OPEN_DIV_INPUT_GROUP,
        HTML_OPEN_DIV_INPUT_GROUP_APPEND,
        HTML_OPEN_SPAN_INPUT_GROUP_TEXT,
        HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY,
        HTML_CLOSE_GROUP_START_HR,
        MASK_NO_FORMAT = 0x7f
    };

    const __FlashStringHelper *getFormatByType(FormatType type) const;


    PrintArgs(const PrintArgs &print) = delete;

    PrintArgs();
    ~PrintArgs();

    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

    // no type conversion: float, double and uint64_t need 2 arguments
    // uint8_t, uint16_t need to be zero padded
    void vprintf_P(const char *format, const uintptr_t **args, size_t numArgs);

    inline void vprintf_P(const uintptr_t **args, size_t numArgs) {
        vprintf_P(*(const char **)&args[0], &args[1], numArgs);
    }
    inline void vprintf_P(uintptr_t **args, size_t numArgs) {
        vprintf_P(*(const char **)&args[0], &args[1], numArgs);
    }
    inline void vprintf_P(const char *format, const char **args, size_t numArgs) {
        vprintf_P(format, (const uintptr_t **)args, numArgs);
    }
    inline void vprintf_P(const char *format, uintptr_t **args, size_t numArgs) {
        vprintf_P(format, (const uintptr_t **)args, numArgs);
    }

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

            _buffer.reserve(_buffer.length() + (sizeof(uint32_t) * (sizeof...(args) + 2)));

#if DEBUG_PRINT_MSC_VER
            _str = String();
            _str.printf("buf=%u ", _buffer.length());
#endif

            size_t counterLen = _buffer.length();
            _buffer.write(0);
            _copy(format);

            DEBUG_PRINT_ARGS_PRINT("num=%u fmt=%p ", (sizeof...(args)), format);
            if ((uint32_t)format > 0x3ff00000) {
                DEBUG_PRINT_ARGS_PRINT("('%-10.10s') ", format);
            }

#if DEBUG_PRINT_MSC_VER
            int k = sizeof...(args);
            _str.printf("buf=%u fmt='%-10.10s...' args=%u ", _buffer.length(), format, k);
#endif

            //_buffer.write(reinterpret_cast<const uint8_t *>(&format), sizeof(const char *));
            _collect(args...);

            // update argument counter
            _buffer[counterLen] = (uint8_t)((_buffer.length() - counterLen - (sizeof(const char *) + sizeof(uint8_t))) / sizeof(uint32_t));

            DEBUG_PRINT_ARGS_PRINT(" outlen=%u\n", _buffer[counterLen]);

#if DEBUG_PRINT_MSC_VER
            _str.printf("buf=%u size=%u ", _buffer.length(), _buffer[counterLen]);
            Serial.println(_str);
            _str = String();
#endif

            // // add counter for arguments and store position in buffer
            // auto counter = _buffer.end();
            // //size_t counterLen = _buffer.length();
            // _buffer.write(1);
            // _writeFormat(format);
            // if (sizeof...(args)) {
            //     size_t startLen = _buffer.length();
            //     _collect(args...);

            //     // update argument counter
            //     *counter += (uint8_t)((_buffer.length() - startLen) / sizeof(uint32_t));
            // }

        }
    }

    template <typename... Args>
    void printf_P(FormatType type, const char *format, uint8_t numArgs, const Args &... args) {
#if DEBUG_PRINT_ARGS
        _printfCalls++;
        _printfArgs += sizeof...(args);
#endif
        __LDBG_assert_printf(sizeof...(args) % numArgs == 0, "numArgs=%u args%%numArgs=%u args=%u fmt=%-16.16s...", numArgs, sizeof...(args) % numArgs, sizeof...(args), format);
        _buffer.write((uint8_t)FormatType::REPEAT_FORMAT);
        _buffer.write(numArgs);
        printf_P(format, std::forward<const Args &>(args)...);
    }

    template <typename... Args>
    void printf_P(const __FlashStringHelper *format, const Args &... args) {
        printf_P(RFPSTR(format), std::forward<const Args &>(args)...);
    }

    void printf_P(FormatType type, char ch) {
        DEBUG_PRINT_ARGS_PRINT("fmt=%u char=%c ", type, ch);
        printf_P(RFPSTR(getFormatByType(FormatType::SINGLE_CHAR)), ch);
    }

    void printf_P(FormatType type, const char *str) {
        DEBUG_PRINT_ARGS_PRINT("fmt=%u str=%p ", type, str);
        printf_P(RFPSTR(getFormatByType(FormatType::SINGLE_STRING)), str);
    }

    void printf_P(FormatType type, const __FlashStringHelper *fpstr) {
        DEBUG_PRINT_ARGS_PRINT("fmt=%u fpstr=%p ", type, fpstr);
        printf_P(FormatType::SINGLE_STRING, RFPSTR(fpstr));
    }

    void printf_P(FormatType type) {
        switch (type) {
            case FormatType::HTML_CLOSE_QUOTE_CLOSE_TAG:
            case FormatType::HTML_CLOSE_TAG:
            case FormatType::HTML_CLOSE_DIV:
            case FormatType::HTML_CLOSE_DIV_2X:
            case FormatType::HTML_CLOSE_DIV_3X:
            case FormatType::HTML_CLOSE_SPAN:
            case FormatType::HTML_CLOSE_SELECT:
            case FormatType::HTML_OPEN_DIV_FORM_GROUP:
            case FormatType::HTML_OPEN_DIV_INPUT_GROUP:
            case FormatType::HTML_OPEN_DIV_INPUT_GROUP_APPEND:
            case FormatType::HTML_OPEN_SPAN_INPUT_GROUP_TEXT:
            case FormatType::HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY:
            case FormatType::HTML_CLOSE_GROUP_START_HR:
                _buffer.write((uint8_t)type);
                DEBUG_PRINT_ARGS_PRINT("fmt=%u switch ", type);
                return;
default:break;//TODO remove
        }
        DEBUG_PRINT_ARGS_PRINT("fmt=%u str=%p ", type, getFormatByType(type));
        printf_P(RFPSTR(getFormatByType(type)));
    }

private:
    int _snprintf_P(uint8_t *buffer, size_t size, uintptr_t **args, uint8_t numArgs);

    // convert to 32bit signed int, basically its padding with 0xff
    inline void _copy(int8_t value8) {
        _copy((int32_t)value8);
    }

    // convert to 32bit signed int
    inline void _copy(int16_t value16) {
        _copy((int32_t)value16);
    }

    // if double is 64bit and float 32, convert to double
    template <typename T, typename std::enable_if<(std::is_same<T, float>::value && sizeof(double) != sizeof(float)), int>::type = 0>
    inline void _copy(T value) {
        _copy((double)value);
    }

    // unsigned types can be zero padded to 32bit
    template <typename T, typename std::enable_if<(sizeof(T) < sizeof(uintptr_t)), int>::type = 0>
    inline void _copy(T arg) {
        _copy((uintptr_t)arg);
    }

    // 32 bit args
    template <typename T, typename std::enable_if<(sizeof(T) == sizeof(uintptr_t)), int>::type = 0>
    inline void _copy(T arg) {
        auto ptr = _buffer.begin();
        _buffer.push_back((uintptr_t)arg);
        DEBUG_PRINT_ARGS_PRINT("copy=%p buf=%p ", reinterpret_cast<const uint8_t *>(arg), (uint32_t)&ptr[0]);
        if ((uint32_t)arg > 0x3ff00000) {
            DEBUG_PRINT_ARGS_PRINT("('%-10.10s') ", reinterpret_cast<const uint8_t *>(arg));
        }
    }

    // 64bit args
    template <typename T, typename std::enable_if<(sizeof(T) == sizeof(uint64_t)), int>::type = 0>
    inline void _copy(T arg) {
        _buffer.push_back((uint64_t)arg);
        DEBUG_PRINT_ARGS_PRINT("copy=%p %p ", reinterpret_cast<const uint8_t *>(&arg));
    }

    // process arguments
    void _collect() {}

    template <typename T>
    void _collect(const T &t) {
#if DEBUG_PRINT_MSC_VER
        _str.printf("type=%s ptr=%p ", typeid(T).name(), t);
#endif
        _copy(t);
    }

    template <typename T, typename... Args>
    void _collect(const T &t, const Args &... args) {
#if DEBUG_PRINT_MSC_VER
        _str.printf("type=%s ptr=%p ", typeid(T).name(), t);
#endif
        _copy(t);
        _collect(args...);
    }

protected:
    static const int PrintObject = -1;

    uint8_t *_bufferPtr;
    int _position;
private:
    Buffer _buffer;
    int _strLength;
#if DEBUG_PRINT_ARGS
    size_t _outputSize;
    size_t _printfCalls;
    size_t _printfArgs;
#if DEBUG_PRINT_ARGS
    void _debugPrint(PGM_P format, ...);
    PrintString _debugStr;
#endif
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
