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
    char ch;
    if (buffer) {
        while (length-- && (ch = static_cast<char>(pgm_read_byte(value++)))) {
            auto result = buffer->feed(ch);
            switch (static_cast<ErrorType>(result)) {
            case ErrorType::MORE_DATA_REQUIRED:
                break;
            case ErrorType::NO_ENCODING_REQUIRED:
                if (escape(ch)) {
                    outputLen += 2;
                }
                else {
                    outputLen++;
                }
                break;
            default:
                outputLen += buffer->unicodeLength(result);
                break;
            case ErrorType::INVALID_SEQUENCE:
            case ErrorType::INVALID_UNICODE_SYMBOL:
                buffer->clear();
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
    char ch;
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
                if (!result) {
                    return outputLen;
                }
                outputLen += result;
            } break;
            case ErrorType::MORE_DATA_REQUIRED:
                break;
            default:
                outputLen += buffer->printTo(output, result);
                // fall through
            case ErrorType::INVALID_SEQUENCE:
            case ErrorType::INVALID_UNICODE_SYMBOL:
                buffer->clear();
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
            if (!result) {
                return outputLen;
            }
            outputLen += result;
        }
    }
    return outputLen;
}
