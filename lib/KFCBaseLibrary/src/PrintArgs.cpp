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
    __LDBG_assert_printf((type >= FormatType::STRINGS_BEGIN && type <= FormatType::STRINGS_END), "type=%u invalid", type);

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
        case FormatType::HTML_CLOSE_TEXTAREA:
            return F("%s</textarea>");
        case FormatType::HTML_LABEL_FOR:
            return F("<label for=\"%s\">%s</label>");
        case FormatType::HTML_LABEL_FOR_COLON:
            return F("<label for=\"%s\">%s:</label>");
        case FormatType::HTML_ATTRIBUTE_STR_STR:
            return F(" %s=\"%s\"");
        case FormatType::HTML_ATTRIBUTE_STR_INT:
            return F(" %s=\"%d\"");
        case FormatType::HTML_OPEN_TEXTAREA:
        return F("<textarea class=\"form-control\" name=\"%s\" id=\"%s\"");
        case FormatType::HTML_OPEN_SELECT:
            return F("<select class=\"form-control\" name=\"%s\" id=\"%s\"");
        case FormatType::HTML_OPEN_TEXT_INPUT:
            return F("<input type=\"text\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\"");
        case FormatType::HTML_OPEN_NUMBER_INPUT:
            return F("<input type=\"number\" class=\"form-control\" name=\"%s\" id=\"%s\" value=\"%s\"");
        case FormatType::HTML_OPEN_GROUP_START_CARD:
            return F("<div class=\"card\"><div class=\"card-header p-1\" id=\"heading-%s\"><h2 class=\"mb-1\"><button class=\"btn btn-link btn-lg collapsed\" type=\"button\" data-toggle=\"collapse\" data-target=\"#collapse-%s\" aria-expanded=\"false\" aria-controls=\"collapse-%s\"><strong>%s</strong></button></h2></div><div id=\"collapse-%s\" aria-labelledby=\"heading-%s\" data-cookie=\"#%s\"");
        case FormatType::HTML_OPEN_GROUP_START_CARD_COLLAPSE_HIDE:
            return F(" class=\"collapse\"><div class=\"card-body\">");
        case FormatType::HTML_OPEN_GROUP_START_CARD_COLLAPSE_SHOW:
            return F(" class=\"collapse show\"><div class=\"card-body\">");
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
        case FormatType::HTML_OPTION_NUM_KEY_SELECTED:
            return F("<option value=\"%d\" selected>%s</option>");
        case FormatType::HTML_OPTION_STR_KEY_SELECTED:
            return F("<option value=\"%s\" selected>%s</option>");
        case FormatType::HTML_OPTION_NUM_KEY:
            return F("<option value=\"%d\">%s</option>");
        case FormatType::HTML_OPTION_STR_KEY:
            return F("<option value=\"%s\">%s</option>");
        case FormatType::HTML_CLOSE_GROUP_START_HR:
            return F("</h5></div><div class=\"col-lg-12\"><hr class=\"mt-0\"></div></div><div class=\"form-group\">");
        default:
            break;
    }
    __LDBG_assert_printf(false, "type=%u missing", type);
    return nullptr;
}



namespace PrintArgsHelper {

    class BufferContext {
    public:
        BufferContext(PrintArgs &printArgs, uint8_t *data, size_t sizeIn) :
            _printArgs(printArgs),
            _strLength(printArgs._strLength),
            _args{},
            _outIterator(data),
            _outBegin(data),
            _outEnd(data + sizeIn),
            _advanceInputBuffer(0),
            _numArgs(0),
            _type(FormatType::NONE)
        {
            _printArgs._initOutput();
        }

        void initContext(uint8_t *buffer)
        {
            _advanceInputBuffer = sizeof(_type);
            auto format = *buffer;
            if (format <= kMaximumPrintfArguments/* == format < FormatType::MAX  */) {

                // buffer: count<FormatType>, format<const char *>, arguments<uintptr_t * count>
                struct Header_t {
                private:
                    std::underlying_type<FormatType>::type count;
                public:
                    uint8_t getNumArgs() const {
                        return count + 1;
                    }
                    static uintptr_t *getArgsSrc(uint8_t *buffer) {
                        return reinterpret_cast<uintptr_t *>(buffer + sizeof(Header_t));
                    }
                };
                static_assert(sizeof(Header_t) == sizeof(FormatType), "invalid size");

                _type = FormatType::VPRINTF;
                // add one to the number of arguments for "format"
                _numArgs = reinterpret_cast<Header_t *>(buffer)->getNumArgs();
                // copy format and arguments from input buffer
                std::copy_n(Header_t::getArgsSrc(buffer), _numArgs, _args);
                // store offset to next item
                _advanceInputBuffer = (sizeof(uintptr_t) * _numArgs) + sizeof(Header_t);

                __PADBG_printf_prefix("fmt='%0.32s' args=%u adv=%u\n", _args[0], _numArgs, _advanceInputBuffer);

            } else if (format & static_cast<uint8_t>(FormatType::VPRINTF_TYPE_MASK)) {

                // type<FormatType: 7bit>, argument count<uint8_t>, arguments <uintptr_t * (count + 1)>
                struct Header_t {
                private:
                    std::underlying_type<FormatType>::type type : 7;
                    uint8_t count;
                public:
                    FormatType getFormatType() const {
                        return static_cast<FormatType>(type);
                    }
                    uint8_t getNumArgs() const {
                        return count + 1;
                    }
                    static uintptr_t *getArgsSrc(uint8_t *buffer) {
                        return reinterpret_cast<uintptr_t *>(buffer + sizeof(Header_t));
                    }
                };
                static_assert(sizeof(Header_t) == sizeof(FormatType) + sizeof(uint8_t), "invalid size");

                _type = FormatType::VPRINTF;
                // get format string
                _args[0] = (uintptr_t)getFormatByType(reinterpret_cast<Header_t *>(buffer)->getFormatType());
                // get number of arguments
                _numArgs = reinterpret_cast<Header_t *>(buffer)->getNumArgs();
                // copy arguments from input buffer
                std::copy_n(Header_t::getArgsSrc(buffer), _numArgs, &_args[1]);
                // store offset to next item
                _advanceInputBuffer = (sizeof(uintptr_t) * _numArgs) + sizeof(Header_t);

                __PADBG_printf_prefix("type_fmt='%0.32s' args=%u adv=%u\n", _args[0], _numArgs, _advanceInputBuffer);

            }
            else {
                _type = static_cast<FormatType>(format);
                if (_type >= FormatType::STRINGS_BEGIN && _type <= FormatType::STRINGS_END) {
                    // predefined strings
                    _str = (char *)getFormatByType(_type);
                    _strLength = strlen_P(_str);
#if DEBUG_PRINT_ARGS
                    _printArgs._stats.maxLen.copyString = std::max<uint16_t>(_printArgs._stats.maxLen.copyString, _strLength);
#endif
                }
                else {
                    switch (_type) {
                        case FormatType::VPRINTF_REPEAT:
                            {
                                __LDBG_assert_printf(false, "not implemented");

                                // buffer: type<FormatType>, format repeat count<uint8_t>, argument count<uint8_t>, format<const char *>, arguments<uintptr_t * count>
                                struct Header_t {
                                private:
                                    FormatType type;
                                    uint8_t fmtRepeat;
                                    uint8_t argCount;
                                public:
                                    uint8_t getRepeatFormat() const {
                                        return fmtRepeat;
                                    }
                                    uint8_t getNumArgs() const {
                                        return argCount / fmtRepeat;
                                    }
                                    static uintptr_t getFormat(uint8_t *buffer) {
                                        return *reinterpret_cast<uintptr_t *>(buffer + sizeof(Header_t));
                                    }
                                    static uintptr_t *getArgsSrc(uint8_t *buffer, uint8_t repeatCount, uint8_t numArgs) {
                                        return reinterpret_cast<uintptr_t *>(buffer + sizeof(Header_t) + sizeof(const char *) + (repeatCount * (sizeof(uintptr_t) * numArgs)));
                                    }
                                };
                                static_assert(sizeof(Header_t) == sizeof(FormatType) + sizeof(uint8_t) * 2, "invalid size");

                                _numArgs = reinterpret_cast<Header_t *>(buffer)->getNumArgs();
                                _args[0] = Header_t::getFormat(buffer);

                                std::copy_n(Header_t::getArgsSrc(buffer, 0/*repeat count*/, _numArgs), _numArgs, &_args[1]);
                                //TODO
                            }
                            break;
                        case FormatType::SINGLE_CHAR:
                            // single character
                            _char[0] = *(buffer + sizeof(FormatType));
                            _str = _char;
                            _advanceInputBuffer += sizeof(*buffer);
                            _strLength = 1;
                            break;
                        case FormatType::SINGLE_CRLF:
                            // println()
                            _char[0] = '\r';
                            _char[1] = '\n';
                            _str = _char;
                            //_advanceInputBuffer += 0;
                            _strLength = 2;
                            break;
                        case FormatType::SINGLE_STRING:
                            // single string from print()/println() without formating
                            std::copy_n(buffer + sizeof(FormatType), sizeof(_str), (uint8_t *)&_str);
                            _advanceInputBuffer += sizeof(_str);
                            _strLength = strlen_P(_str);
#if DEBUG_PRINT_ARGS
                            _printArgs._stats.maxLen.copyString = std::max<uint16_t>(_printArgs._stats.maxLen.copyString, _strLength);
#endif
                            break;
                        default:
                            __LDBG_assert_printf(false, "type %u not implemented", _type);
                            break;
                    }
                }
                __PADBG_printf_prefix("type=%u strlen=%u adv=%u\n", _type, _strLength, _advanceInputBuffer);
            }
        }

        inline bool isPrintf() const {
            return _type == FormatType::VPRINTF;
        }

        // size/used capacity
        inline size_t size() const {
            return _outIterator - _outBegin;
        }

        // remaining capacity
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
#if DEBUG_PRINT_ARGS
                _printArgs._stats.maxLen.vprintf = std::max<uint16_t>(_printArgs._stats.maxLen.vprintf, _strLength);
#endif
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
#if DEBUG_PRINT_ARGS
                _printArgs._stats.calls.vprintf++;
#endif
            }
            else {
                written = copyString(offset);
#if DEBUG_PRINT_ARGS
                _printArgs._stats.calls.copyString++;
#endif
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
            // arguments for vprintf
            uintptr_t _args[PrintArgs::kMaximumPrintfArguments + 1];
            // data for copyString
            struct {
                char *_str;
                char _char[4];
            };
        };
        // output buffer
        uint8_t *_outIterator;
        uint8_t *_outBegin;
        uint8_t *_outEnd;
        // increment to read next item from input buffer
        uint8_t _advanceInputBuffer;
        // number of arguments for vprintf (_args[])
        uint8_t _numArgs;
        // current format type
        FormatType _type;
    };

}

void PrintArgs::clear()
{
#if DEBUG_PRINT_ARGS

    if (_stats.outputSize) {
        __LDBG_printf("calls: copystr=%u[repeated=%u] vprintf=%u[%u] fillbuffer=%u ",
            _stats.calls.copyString, _stats.repeat.copyString, _stats.calls.vprintf, _stats.repeat.vprintf, _stats.calls.fillBuffer
        );
        __LDBG_printf("output=%u buffer_capacity=%u max_len: vprintf=%u copystr=%u",
            _stats.outputSize, _buffer.capacity(), _stats.maxLen.vprintf, _stats.maxLen.copyString
        );
        PrintString str;
        for (uint8_t i = 0; i < kMaximumPrintfArguments + 1; i++) {
            if (_stats.calls.numArgs[i]) {
                str.printf_P(PSTR("args[%u]=%u "), i, _stats.calls.numArgs[i]);
            }
        }
        if (str.length()) {
            __LDBG_printf("vprintf num.%s", str.c_str());
        }
    }
    _stats = {};
#endif
    _buffer.clear();
    _bufferPtr = nullptr;
}

using BufferContext = PrintArgsHelper::BufferContext;

size_t PrintArgs::fillBuffer(uint8_t *data, size_t sizeIn)
{
#if DEBUG_PRINT_ARGS
    _stats.calls.fillBuffer++;
#endif
    BufferContext ctx(*this, data, sizeIn);

    while(ctx._outIterator < ctx._outEnd) {

        // input buffer empty
        if (_bufferPtr >= _buffer.end()) {
            __PADBG_printf_prefix("end of input buffer\n");
#if DEBUG_PRINT_ARGS
            // add last chunk, print debug info and clear the buffer
            _stats.outputSize += ctx.size();
            clear();
            return ctx.size();
#else
            clear();
            break;
#endif
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
#if DEBUG_PRINT_ARGS
            if (ctx.isPrintf()) {
                _stats.repeat.vprintf++;
            }
            else {
                _stats.repeat.copyString++;
            }
#endif
            //__LDBG_assert_printf((ctx._outEnd == ctx._outIterator) || (ctx._outEnd - ctx._outIterator == 1), "out of range=%d", ctx._outEnd - ctx._outIterator);
            break;
        }
    }
#if DEBUG_PRINT_ARGS
    _stats.outputSize += ctx.size();
#endif
    return ctx.size();
}
