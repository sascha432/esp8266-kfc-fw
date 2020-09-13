/**
* Author: sascha_lammers@gmx.de
*/

#include "PrintArgs.h"

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif


#if defined(ESP8266)
#include "umm_malloc/umm_malloc_cfg.h"
#endif


const __FlashStringHelper *PrintArgs::getFormatByType(FormatType type) const
{
    switch(type) {
        case FormatType::SINGLE_STRING:
            return F("%s");
        case FormatType::SINGLE_CHAR:
            return F("%c");
        case FormatType::HTML_CLOSE_QUOTE_CLOSE_TAG:
            return F("\">");
        case FormatType::HTML_CLOSE_TAG:
            return F(">");
        case FormatType::HTML_CLOSE_DIV:
            return F("</div>");
        case FormatType::HTML_CLOSE_DIV_2X:
            return F("</div></div></div>");
        case FormatType::HTML_CLOSE_DIV_3X:
            return F("</div></div>");
        case FormatType::HTML_CLOSE_SPAN:
            return F("</span>");
        case FormatType::HTML_CLOSE_SELECT:
            return F("</select>");
        case FormatType::HTML_CLOSE_LABEL:
            return F("</label>");
        case FormatType::HTML_CLOSE_LABEL_WITH_COLON:
            return F(":</label>");
        case FormatType::HTML_OPEN_DIV_FORM_GROUP:
            return F("<div class=\"form-group\">");
        case FormatType::HTML_OPEN_DIV_INPUT_GROUP:
            return F("<div class=\"input-group\">");
        case FormatType::HTML_OPEN_DIV_INPUT_GROUP_APPEND:
            return F("<div class=\"input-group-append\">");
        case FormatType::HTML_OPEN_SPAN_INPUT_GROUP_TEXT:
            return F("<span class=\"input-group-text\">");
        case FormatType::HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY:
            return F("<div class=\"card\"><div><div class=\"card-body\">");
        case FormatType::HTML_CLOSE_GROUP_START_HR:
            return F("</h5></div><div class=\"col-lg-12\"><hr class=\"mt-0\"></div></div><div class=\"form-group\">");

default://TODO remove default
        case FormatType::NONE:
        case FormatType::MAX:
            break;
    }
    return nullptr;
}

PrintArgs::PrintArgs() : _bufferPtr(nullptr), _position(0), _strLength(0)
{
#if DEBUG_PRINT_ARGS
    _outputSize = 0;
    _printfCalls = 0;
    _printfArgs = 0;
#endif
}

PrintArgs::~PrintArgs()
{
}

void PrintArgs::clear()
{
#if DEBUG_PRINT_ARGS
    if (_outputSize) {
        __LDBG_printf("out=%d buffer=%d calls=%u args=%u", _outputSize, _buffer.size(), _printfCalls, _printfArgs);
        _outputSize = 0;
    }
#endif
    if (_buffer.size()) {
        __LDBG_printf("this=%p buffer=%d", this, _buffer.size());
        _buffer.clear();
    }
    _bufferPtr = nullptr;
}

size_t PrintArgs::fillBuffer(uint8_t *data, size_t sizeIn)
{
    int size = sizeIn;
    std::array<uintptr_t *, kMaximumPrintfArguments + 1> args;
    static_assert(args.size() >= (uint8_t)FormatType::MAX, "invalid size");
    Buffer temp;

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
        uint8_t numArgs = *_bufferPtr & static_cast<uint8_t>(FormatType::MASK_NO_FORMAT);
        // print a list of strings instead using printf
        bool noFormat = *_bufferPtr & ~static_cast<uint8_t>(FormatType::MASK_NO_FORMAT);
        auto src = reinterpret_cast<uintptr_t **>(_bufferPtr + 1);
        size_t advance = 1;

        if (numArgs >= (uint8_t)FormatType::MAX) {
            auto formatType = static_cast<FormatType>(numArgs);
            switch(formatType) {

                //TODO
                // case FormatType::SINGLE_CHAR:
                //     break;
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
                    {
                        numArgs = 0; // no arguments
                        args[0] = reinterpret_cast<uintptr_t *>(const_cast<__FlashStringHelper *>(getFormatByType(formatType)));
                        advance = 1;
                        src = &args[0];
                    }
                    break;
                case FormatType::SINGLE_CHAR:
                    {
                        numArgs = 1;
                        args[0] = reinterpret_cast<uintptr_t *>(const_cast<__FlashStringHelper *>(getFormatByType(FormatType::SINGLE_CHAR)));
                        args[1] = reinterpret_cast<uintptr_t *>(*_bufferPtr + 1);
                        advance += 1;
                        src = &args[0];
                    }
                    break;
                case FormatType::SINGLE_STRING:
                    {
                        numArgs = 1;
                        args[0] = reinterpret_cast<uintptr_t *>(const_cast<__FlashStringHelper *>(getFormatByType(FormatType::SINGLE_STRING)));
                        args[1] = reinterpret_cast<uintptr_t *>(*_bufferPtr + 1);
                        advance += sizeof(const char *);
                        src = &args[0];
                    }
                    break;

default://TODO remove default
                case FormatType::NONE:
                case FormatType::MAX:
                case FormatType::ENCODE_HTML_ATTRIBUTE:
                case FormatType::ENCODE_HTML_ENTITIES:
                case FormatType::ENCODE_JSON:
                case FormatType::ESCAPE_JAVASCRIPT:
                case FormatType::REPEAT_FORMAT:
                    clear();
                    return 0;
            }
            numArgs++;
        }
        else {
            advance += sizeof(uintptr_t) * ++numArgs;
            //if (src != args.data()) {
            std::copy_n(src, numArgs, args.begin());
            //}
        }
#if DEBUG_PRINT_ARGS
        std::fill(args.begin() + numArgs, args.end(), nullptr);
#endif

#if DEBUG_PRINT_MSC_VER
        PrintString str;
        str.printf_P(PSTR("fmt='%-10.10s...' args=%u buf=%u"), args[0], numArgs - 1, _buffer.length());
        for (int i = 1; i < numArgs; i++) {
            str.printf_P(PSTR("idx=%u ptr=%p str='%-10.10s'"), i, args[i], args[i]);
        }
        Serial.println(str);
#endif

        // sprintf starts at position 0
        if (_position == 0) {
            _strLength = _snprintf_P(data, size, args.data(), numArgs);
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
                /*_strLength = */_snprintf_P(data, _strLength + 1, args.data(), numArgs);
                memmove(data, data + _position, left);
                data += left;
                size -= left;
                _bufferPtr += advance;
                _position = 0;
                continue;
            }
            // we need extra space
            int maxTmpLen = std::min(_strLength, size + _position) + 1;
            temp.reserve(maxTmpLen);
            /*_strLength = */_snprintf_P(temp.begin(), maxTmpLen, args.data(), numArgs);
            if (left <= size) {
                memcpy(data, temp.begin() + _position, left);
                data += left;
                size -= left;
                _bufferPtr += advance;
                _position = 0;
                continue;
            }
            else {
                memcpy(data, temp.begin() + _position, size);
                data += size;
                _position += size;
                size = 0;
                continue;
            }
        }
    } while (true);
}

void PrintArgs::vprintf_P(const char *format, const uintptr_t **args, size_t numArgs)
{
#if DEBUG_PRINT_ARGS
    _printfCalls++;
    _printfArgs += numArgs;
#endif
    _buffer.write((uint8_t)numArgs);
    _buffer.push_back((uintptr_t)format);
    _buffer.write(reinterpret_cast<const uint8_t *>(&args[0]), sizeof(uintptr_t) * numArgs);
}


int PrintArgs::_snprintf_P(uint8_t *buffer, size_t size, uintptr_t **args, uint8_t numArgs)
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

    return snprintf_P(reinterpret_cast<char *>(buffer), size, (PGM_P)args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);
}

//void PrintArgs::_copy(const __FlashStringHelper *fpstr)
//{
//    _writeFormat(fpstr);
//    //_buffer.write(reinterpret_cast<const uint8_t *>(&fpstr), sizeof(const char *));
//}

//void PrintArgs::_copy(const char *str)
//{
//    _writeFormat(str);
//// #if defined(ESP8266) && 0
////     if (is_PGM_P(str))
////         _buffer.write(reinterpret_cast<const uint8_t *>(&str), sizeof(const char *));
////     }
////     else {
////         auto copy = _strings.attachString(str);
////         _buffer.write(reinterpret_cast<const uint8_t *>(&copy), sizeof(const char *));
////     }
//// #else
////     _buffer.write(reinterpret_cast<const uint8_t *>(&str), sizeof(const char *));
//// #endif
//}
//
//void PrintArgs::_writeFormat(const void *format)
//{
//#if defined(ESP8266) && 0
//    uint16_t value = (uintptr_t)format - SECTION_HEAP_START_ADDRESSS;
//    _buffer.write(reinterpret_cast<const uint8_t *>(&value), sizeof(value));
//#else
//    _buffer.write(reinterpret_cast<const uint8_t *>(&format), sizeof(const char *));
//#endif
//}
//

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
