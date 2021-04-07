/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonTools.h"
#include "JsonVar.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

const __FlashStringHelper *JsonTools::boolToString(bool value)
{
    return value ? FSPGM(true) : FSPGM(false);
}

size_t JsonTools::lengthEscaped(PGM_P value, size_t length, Utf8Buffer *buffer)
{
    if (!length) {
        return 0;
    }
    size_t outputLen = 0;
    char ch = 0;
    if (buffer) {
        while (length-- && (ch = static_cast<char>(pgm_read_byte(value++)))) {
            auto codepoint = buffer->feed(ch);
            switch (static_cast<ErrorType>(codepoint)) {
            case ErrorType::NO_ENCODING_REQUIRED:
                if (escape(ch)) {
                    outputLen += 2;
                }
                else {
                    outputLen++;
                }
                break;
            case ErrorType::MORE_DATA_REQUIRED:
            case ErrorType::INVALID_SEQUENCE:
            case ErrorType::INVALID_UNICODE_SYMBOL:
                break;
            default:
                outputLen += buffer->unicodeLength(codepoint);
                break;
            }
        }
    }
    else {
        while (length-- && (ch = static_cast<char>(pgm_read_byte(value++)))) {
            if (escape(ch)) {
                outputLen += 2;
            }
            else {
                outputLen++;
            }
        }
    }
    return outputLen;
}

size_t JsonTools::printToEscaped(Print &output, PGM_P value, size_t length, Utf8Buffer *buffer)
{
    if (!length) {
        return 0;
    }
    size_t outputLen = 0;
    char ch = 0;
    if (buffer) {
        while (length-- && (ch = static_cast<char>(pgm_read_byte(value++)))) {
            auto result = buffer->feed(ch);
            switch (static_cast<ErrorType>(result)) {
            case ErrorType::NO_ENCODING_REQUIRED: {
                size_t result;
                if (escape(ch)) {
                    result = printEscaped(output, ch);
                }
                else {
                    result = output.print(ch);
                }
                outputLen += result;
            } break;
            case ErrorType::MORE_DATA_REQUIRED:
            case ErrorType::INVALID_SEQUENCE:
            case ErrorType::INVALID_UNICODE_SYMBOL:
                break;
            default:
                outputLen += buffer->printTo(output, result);
                break;
            }
        }
    }
    else {
        size_t result;
        while (length-- && (ch = static_cast<char>(pgm_read_byte(value++)))) {
            if (escape(ch)) {
                result = printEscaped(output, ch);
            }
            else {
                result = output.print(ch);
            }
            outputLen += result;
        }
    }
    return outputLen;
}

int32_t JsonTools::Utf8Buffer::feed(uint8_t ch)
{
    // no encoding required
    if (_counter == 0) {
        if (ch < 0x80) {
            // 7bit ascii
            return static_cast<int32_t>(ErrorType::NO_ENCODING_REQUIRED);
        }
        if (utf8_length(ch) > 3) {
            // data byte received
            return static_cast<int32_t>(ErrorType::INVALID_SEQUENCE);
        }
        // store length byte
        _counter = 1;
        *_buf = ch;
        return static_cast<int32_t>(ErrorType::MORE_DATA_REQUIRED);
    }
    if (isNotData(ch)) {
        // length byte or 7bit received
        _counter = 0;
        return static_cast<int32_t>(ErrorType::INVALID_SEQUENCE);
    }
    uint8_t len = utf8_length(*_buf);
    if (_counter == len) {
        _counter = 0;
        switch (len) {
        case 3: { // 4 byte sequence
            uint32_t tmp = ((_buf[0] & 0b111) << 18) | ((_buf[1] & 0b111111) << 12) | ((_buf[2] & 0b111111) << 6) | (ch & 0b111111);
            if (tmp <= 0x10ffff) {
                return tmp;
            }
            // invalid unicode
            return static_cast<int32_t>(ErrorType::INVALID_UNICODE_SYMBOL);
        }
        case 2: // 3 byte sequence
            return ((_buf[0] & 0b1111) << 12) | ((_buf[1] & 0b111111) << 6) | (ch & 0b111111);
        case 1: // 2 byte sequence
            return ((_buf[0] & 0b11111) << 6) | (ch & 0b111111);
        }
        // should not be reached
        return static_cast<int32_t>(ErrorType::INVALID_SEQUENCE);
    }
    _buf[_counter++] = ch;
    return static_cast<int32_t>(ErrorType::MORE_DATA_REQUIRED);
}
