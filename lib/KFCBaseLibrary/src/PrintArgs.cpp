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


const __FlashStringHelper *PrintArgsHelper::getFormatByType(FormatType type)
{
    __LDBG_assert_printf((type >= FormatType::HTML_OUTPUT_BEGIN && type <= FormatType::HTML_OUTPUT_END), "type=%u invalid", type);

    switch(type) {
        case FormatType::HTML_CLOSE_QUOTE_CLOSE_TAG:
            return F("\">");
        case FormatType::HTML_CLOSE_TAG:
            return F(">");
        case FormatType::HTML_CLOSE_DIV:
            return F("</div>");
        case FormatType::HTML_CLOSE_DIV_2X:
            return F("</div></div>");
        case FormatType::HTML_CLOSE_DIV_3X:
            return F("</div></div></div>");
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
        case FormatType::HTML_OPEN_DIV_INPUT_GROUP_TEXT:
            return F("<div class=\"input-group-text\">");
        case FormatType::HTML_OPEN_DIV_CARD_DIV_DIV_CARD_BODY:
            return F("<div class=\"card\"><div><div class=\"card-body\">");
        case FormatType::HTML_CLOSE_GROUP_START_HR:
            return F("</h5></div><div class=\"col-lg-12\"><hr class=\"mt-0\"></div></div><div class=\"form-group\">");
        default:
            break;
    }
    return nullptr;
}

namespace PrintArgsHelper {

    class BufferContext {
    public:
        BufferContext(PrintArgs &printArgs, uint8_t *data, size_t sizeIn) :
            _printArgs(printArgs),
            _strLength(printArgs._strLength),
            _outIterator(data),
            _outBegin(data),
            _outEnd(data + sizeIn),
            _advanceInputBuffer(0)
        {
            _printArgs._initOutput();
        }

        void initContext(uint8_t *buffer)
        {
            _advanceInputBuffer = sizeof(_type);
            _type = static_cast<FormatType>(*buffer);
            if (_type >= FormatType::MAX) {

                __LDBG_assert_printf(_type < FormatType::MASK_NO_FORMAT, "not implemented");

                if (_type >= FormatType::HTML_OUTPUT_BEGIN && _type <= FormatType::HTML_OUTPUT_END) {
                    // predefined string
                    _str = (char *)getFormatByType(_type);
                    _strLength = strlen_P(_str);
                }
                else if (_type == FormatType::SINGLE_CHAR) {
                    // single char argument, create a string
                    _char[0] = buffer[1];
                    _str = _char;
                    _advanceInputBuffer += sizeof(*buffer);
                    _strLength = 1;
                }
                else if (_type == FormatType::SINGLE_CRLF) {
                    // single char argument, create a string
                    _char[0] = '\r';
                    _char[1] = '\n';
                    _str = _char;
                    _advanceInputBuffer += sizeof(*buffer);
                    _strLength = 2;
                }
                else if (_type == FormatType::SINGLE_STRING) {
                    // single string
                    std::copy_n(buffer + 1, sizeof(_str), (uint8_t *)&_str);
                    _advanceInputBuffer += sizeof(_str);
                    _strLength = strlen_P(_str);
                }
                else {
                    __LDBG_assert_printf(false, "invalid type %u", _type);
                }
                __PADBG_printf_prefix("type=%u strlen=%u adv=%u\n", _type, _strLength, _advanceInputBuffer);

            }
            else {
                // get number of arguments for print
                _numArgs = static_cast<uint8_t>(_type) + 1;
                // advance input buffer when done
                _advanceInputBuffer += (sizeof(uintptr_t) * _numArgs);
                // copy arguments from input buffer
                std::copy_n(reinterpret_cast<uintptr_t **>(buffer + 1), _numArgs, _args);
                _type = FormatType::VPRINTF;

                __PADBG_printf_prefix("fmt='%0.32s' args=%u adv=%u\n", _args[0], _numArgs, _advanceInputBuffer);
            }
        }

        inline bool isPrintf() const {
            return _type == FormatType::VPRINTF;
        }

        inline size_t size() const {
            return _outIterator - _outBegin;
        }

        inline int capacity() const {
            return _outEnd - _outIterator;
        }

        int copyString(size_t offset)
        {
            auto toCopy = _printArgs._strLength - offset;
            auto end = _outIterator + toCopy;
            if (end >= _outEnd) {
                end = _outEnd;
            }
            auto written = end - _outIterator;
            memcpy_P(_outIterator, _str + offset, written);
            _outIterator += written;
            return written;
        }

        int vprintf(size_t offset)
        {
            auto maxCapacity = capacity();
            int written = _printArgs._snprintf_P(_outIterator, maxCapacity, _args, _numArgs);
            if (offset == 0) {
                // update string length when starting with a zero offset
                _strLength = written;
            }
            // printf adds a NUL byte, capacity is reduced
            written = std::min(written, maxCapacity - 1);
            if (offset) {
                // if we have an offset, move data to the beginning
                written -= offset;
                __LDBG_assert_printf(written >= 0, "written=%d negative", written);
                std::copy_n(_outIterator + offset, written, _outIterator);
            }
            _outIterator += written;
            return written;
        }

        int copyBuffer(int offset) {
            int written;
            if (isPrintf()) {
                written = vprintf(offset);
            }
            else {
                written = copyString(offset);
            }
            __LDBG_assert_printf(written <= _strLength - offset, "written=%d > data=%d", written, _strLength - offset);
            return written;
        }

        uint8_t *advanceInputBuffer()
        {
            _printArgs._position = 0;
            _printArgs._bufferPtr += _advanceInputBuffer;
            _advanceInputBuffer = 0;
            return _printArgs._bufferPtr;
        }

    private:
        friend PrintArgs;

        PrintArgs &_printArgs;
        // size of data to be copied to output buffer
        int &_strLength;
        union {
            // arguments for printf
            uintptr_t *_args[PrintArgs::kMaximumPrintfArguments + 1];
            // data for copyString
            struct {
                char *_str;
                char _char[4];
            };
        };
        uint8_t *_outIterator;
        uint8_t *_outBegin;
        uint8_t *_outEnd;
        uint16_t _advanceInputBuffer;
        uint8_t _numArgs;
        // current format type
        FormatType _type;
    };

}

void PrintArgs::clear()
{
    if (_buffer.size()) {
        _buffer.clear();
    }
    _bufferPtr = nullptr;
}

using BufferContext = PrintArgsHelper::BufferContext;

size_t PrintArgs::fillBuffer(uint8_t *data, size_t sizeIn)
{
    BufferContext ctx(*this, data, sizeIn);

    while(ctx._outIterator < ctx._outEnd) {

        // input buffer empty
        if (_bufferPtr >= _buffer.end()) {
            __PADBG_printf_prefix("end of input buffer\n");
            clear();
            break;
        }

        // read type and format from buffer
        ctx.initContext(_bufferPtr);

        int written;

        if (_position == 0) {
            // a new string starts
            written = ctx.copyBuffer(_position);
        }
        else {
            // printf cannot start with an offset, so we need to make sure the buffer has enough capacity
            if (ctx.isPrintf()) {
                if (_strLength >= ctx.capacity() - 1) {
                    // TODO
                    // in case the output buffer is too small, create temporary buffer for printf
                    // or increase the output buffer if possible
                    __LDBG_assert_printf(ctx._outIterator != ctx._outBegin, "output buffer=%u too small for the data=%u", ctx.capacity(), _strLength + 1);
                    // not enough space, return what we have and wait for an empty buffer
                    break;
                }
            }
            written = ctx.copyBuffer(_position);
        }

        __LDBG_assert_printf(written <= _strLength - _position, "written=%d > data=%d", written, _strLength - _position);
        if (written == _strLength - _position) {
            // all data fit into the output buffer, continue with next item
            ctx.advanceInputBuffer();
            continue;
        }
        else {
            // buffer full, remember last position
            _position += written;
            //__LDBG_assert_printf((ctx._outEnd == ctx._outIterator) || (ctx._outEnd - ctx._outIterator == 1), "out of range=%d", ctx._outEnd - ctx._outIterator);
            break;
        }
    }
    return ctx.size();
}

void PrintArgs::vprintf_P(const char *format, const uintptr_t **args, size_t numArgs)
{
    _buffer.write((uint8_t)numArgs);
    _buffer.push_back((uintptr_t)format);
    _buffer.write(reinterpret_cast<const uint8_t *>(&args[0]), sizeof(uintptr_t) * numArgs);
}
