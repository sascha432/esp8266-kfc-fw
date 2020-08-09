/**
* Author: sascha_lammers@gmx.de
*/

#include "PrintArgs.h"

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif


// PrintArgs::CharPtrContainer::CharPtrContainer(CharPtrContainer &&str) noexcept
// {
//     *this = std::move(str);
// }

// PrintArgs::CharPtrContainer &PrintArgs::CharPtrContainer::operator=(CharPtrContainer &&str) noexcept
// {
//     _str = str._str;
//     str._str = nullptr;
// #if !defined(ESP8266)
//     _free = str._free;
//     str._free = false;
// #endif
//     return *this;
// }

// #if defined(ESP8266)

// PrintArgs::CharPtrContainer::CharPtrContainer(char *str, bool) : _str(str)
// {
// }

// PrintArgs::CharPtrContainer::CharPtrContainer(const __FlashStringHelper *str) : _str(reinterpret_cast<char *>(const_cast<__FlashStringHelper *>(str)))
// {
// }

// #else

// PrintArgs::CharPtrContainer::CharPtrContainer(char *str, bool free) : _str(str), _free(free)
// {
// }

// PrintArgs::CharPtrContainer::CharPtrContainer(const __FlashStringHelper *str) : _str(reinterpret_cast<char *>(const_cast<__FlashStringHelper *>(str))), _free(false)
// {
// }

// #endif

// PrintArgs::CharPtrContainer::operator const char *() const
// {
//     return _str;
// }

// const char *PrintArgs::CharPtrContainer::c_str() const
// {
//     return _str;
// }

// bool PrintArgs::CharPtrContainer::operator==(const char *str) const
// {
//     return strcmp_P_P(_str, str) == 0; // function is nullptr sdafe
// }

// bool PrintArgs::CharPtrContainer::operator!=(const char *str) const
// {
//     return strcmp_P_P(_str, str);
// }

// PrintArgs::CharPtrContainer::~CharPtrContainer()
// {
// #if defined(ESP8266)
//     // detect if string is allocated
//     if (_str && !is_PGM_P(_str)) {
//         free(_str);
//     }
// #else
//     if (_free) {
//         free(_str);
//     }
// #endif
// }


PrintArgs::PrintArgs() : _bufferPtr(nullptr), _position(0), _strLength(0), _strings(0)
{
#if DEBUG_PRINT_ARGS
    _outputSize = 0;
    _printfCalls = 0;
    _printfArgs = 0;
#endif
}

PrintArgs::~PrintArgs()
{
#if DEBUG_PRINT_ARGS
    // __DBG_print("--attached---");
    // for(const auto &str: _attached) {
    //     __DBG_printf("%p=%u %s", str.c_str(), is_PGM_P(str.c_str()), str.c_str());
    // }
    // __DBG_print("--end---");
#endif
}

void PrintArgs::clear()
{
#if DEBUG_PRINT_ARGS
    if (_outputSize) {
        __LDBG_printf("out=%d buffer=%d calls=%u args=%u", _outputSize, _buffer.size(), _printfCalls, _printfArgs);
        _outputSize = 0;
    }
#endif
    // __LDBG_printf("this=%p buffer=%d attached=%u/%u", this, _buffer.size(), _strings.size(), _strings.count());
    __LDBG_printf("this=%p buffer=%d", this, _buffer.size());
    // _strings.clear();
    _buffer.clear();
    _bufferPtr = nullptr;
}

size_t PrintArgs::fillBuffer(uint8_t *data, size_t sizeIn)
{
    int size = sizeIn;

    void *args[kMaximumPrintfArguments + 1];
    if (!_bufferPtr) {
        _bufferPtr = _buffer.begin();
        _position = 0;
#if DEBUG_PRINT_ARGS
        _outputSize = 0;
#endif
    }
    auto dataStart = data;
    // _debug_printf("_buffer %p-%p, ptr %p position %d\n", _buffer.begin(), _buffer.end(), _bufferPtr, _position);
    do {
        if (size == 0) {
#if DEBUG_PRINT_ARGS
            _outputSize += (data - dataStart);
#endif
            return data - dataStart;
        }
        if (_bufferPtr >= _buffer.end()) {
            clear();
#if DEBUG_PRINT_ARGS
            _outputSize += (data - dataStart);
#endif
            return data - dataStart;
        }
        uint8_t numArgs = *_bufferPtr;
        if (++numArgs >= (sizeof(args) / sizeof(uintptr_t))) {
            clear();
            return 0;
        }
        size_t advance = sizeof(uintptr_t) * numArgs;
        memset(args, 0, sizeof(args));
        memcpy(args, _bufferPtr + 1, advance++);

        // sprintf starts at position 0
        if (_position == 0) {
            _strLength = _snprintf_P(data, size, args, numArgs);
            if (_strLength < size) {
                // fits into buffer, advance all pointers to the next string
                size -= _strLength;
                data += _strLength;
                _bufferPtr += advance;
                continue;
            }
            else {
                // buffer full, remember last position
                --size; // snprintf writes a NUL byte at the end of the buffer
                data += size;
                _position += size;
                size = 0;
                continue;
            }
        }
        else {
            // we have an offset from previous writes
            int left = _strLength - _position;
            // does the entire string fit?
            if (size - _position >= _strLength + 1) {
                /*_strLength = */_snprintf_P(data, _strLength + 1, args, numArgs);
                memmove(data, data + _position, left);
                data += left;
                size -= left;
                _bufferPtr += advance;
                _position = 0;
                continue;
            }
            // we need extra space
            int maxTmpLen = std::min(_strLength, size + _position) + 1;
            uint8_t *tmp = (uint8_t *)malloc(maxTmpLen);
            /*_strLength = */_snprintf_P(tmp, maxTmpLen, args, numArgs);
            if (left <= size) {
                memcpy(data, tmp + _position, left);
                free(tmp);
                data += left;
                size -= left;
                _bufferPtr += advance;
                _position = 0;
                continue;
            }
            else {
                memcpy(data, tmp + _position, size);
                free(tmp);
                data += size;
                _position += size;
                size = 0;
                continue;
            }
        }
    } while (true);
}

void PrintArgs::vprintf_P(const char *format, uintptr_t **args, size_t numArgs)
{
#if DEBUG_PRINT_ARGS
    _printfCalls++;
    _printfArgs += numArgs;
#endif
    _buffer.write((uint8_t)(numArgs + 1));
    _buffer.write(reinterpret_cast<const uint8_t *>(&format), sizeof(const char *));
    _buffer.write(reinterpret_cast<const uint8_t *>(&args), sizeof(void *) * numArgs);
}


int PrintArgs::_snprintf_P(uint8_t *buffer, size_t size, void **args, uint8_t numArgs)
{
#if DEBUG_PRINT_ARGS && 0
    String fmt = F("format='%s' args[");
    fmt += String((numArgs - 1));
    fmt += "]={";
    for(uint8_t i = 1; i < numArgs; i++) {
        fmt += F("%08x, ");
    }
    String_rtrim_P(fmt, F(", "));
    fmt += F("}\n");
    debug_printf(fmt.c_str(), (PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
#endif

#if defined(_MSC_VER)
    // push all arguments to the stack at once
    typedef struct {
        void *args[kMaximumPrintfArguments + 1];
    } Args_t;
    typedef int(* snprintf_P_stack)(char *str, size_t strSize, Args_t args);
    return ((snprintf_P_stack)(snprintf_P))(reinterpret_cast<char *>(buffer), size, *(Args_t *)(&args[0]));
#else
    return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
#endif
}

void PrintArgs::_copy(float value)
{
    _copy((double)value);
}

// signed integers need to be converted
void PrintArgs::_copy(int8_t value8)
{
    int32_t value = value8;
    _buffer.write(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
}

void PrintArgs::_copy(int16_t value16)
{
    int32_t value = value16;
    _buffer.write(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
}

void PrintArgs::_copy(const __FlashStringHelper *fpstr)
{
    _buffer.write(reinterpret_cast<const uint8_t *>(&fpstr), sizeof(const char *));
}

void PrintArgs::_copy(const char *str)
{
#if defined(ESP8266) && 0
    if (is_PGM_P(str))
        _buffer.write(reinterpret_cast<const uint8_t *>(&str), sizeof(const char *));
    }
    else {
        auto copy = _strings.attachString(str);
        _buffer.write(reinterpret_cast<const uint8_t *>(&copy), sizeof(const char *));
    }
#else
    _buffer.write(reinterpret_cast<const uint8_t *>(&str), sizeof(const char *));
#endif
}

// void PrintArgs::dump(Print &output)
// {
//     int outLen = 0;
//     int calls = 0;
//     int argCount = 0;
//     int maxPrintSize = 0;
//     void *args[8];
//     auto ptr = _buffer.begin();
//     while (ptr < _buffer.end()) {
//         uint8_t num = *ptr++;
//         if (++num >= 8) {
//             break;
//         }
//         memset(args, 0, sizeof(args));
//         memcpy(args, ptr, num * sizeof(ArgPtr));
//         ptr += num * sizeof(ArgPtr);
//         argCount += num;
//         calls++;
//         output.print('|');
//         int n = output.printf_P((PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
//         output.println('|');
//         maxPrintSize = std::max(maxPrintSize, n);
//         outLen += n;
//     }
//     output.println();
//     output.printf_P(PSTR("buffer len=%u print calls=%u arguments=%u output len=%u max print size=%u\n"), _buffer.start_length(), calls, argCount, outLen, maxPrintSize);
//     clear();
// }
