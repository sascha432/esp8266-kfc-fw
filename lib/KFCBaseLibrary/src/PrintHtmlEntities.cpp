/**
 * Author: sascha_lammers@gmx.de
 */

//#include <Arduino_compat.h>
#include "PrintHtmlEntities.h"
#include "PrintHtmlEntitiesString.h"

// index of the character must match the index of string in the values array
const char __keys_attribute_all_P[] PROGMEM = { "'\"<>&" };

const char __keys_javascript_P[] PROGMEM = { "'\"\\\b\f\n\r\t\v" };

// // index of the character must match the index of string in the values array
// const char __keys_html_P[] PROGMEM =  { "'\"<>&%=?#" PRINTHTMLENTITIES_COPY PRINTHTMLENTITIES_DEGREE PRINTHTMLENTITIES_PLUSM PRINTHTMLENTITIES_ACUTE PRINTHTMLENTITIES_MICRO };

// #ifndef _MSC_VER
// static_assert(strncmp(__keys_attribute_all_P, __keys_html_P, strlen(__keys_attribute_all_P)) == 0, "invalid order");
// #endif

// https://en.wikipedia.org/wiki/Windows-1252
const char *__values_P[] PROGMEM = {
    SPGM(htmlentities_apos, "&apos;"),      // '
    SPGM(htmlentities_quot, "&quot;"),      // "
    SPGM(htmlentities_lt, "&lt;"),          // <
    SPGM(htmlentities_gt, "&gt;"),          // >
    SPGM(htmlentities_amp, "&amp;"),        // &
    SPGM(htmlentities_percnt, "&percnt;"),  // %
    SPGM(htmlentities_equals, "&equals;"),  // =
    SPGM(htmlentities_quest, "&quest;"),    // ?
    SPGM(htmlentities_num, "&num;"),        // #
    SPGM(htmlentities_copy, "&copy;"),      // 0xa9 = 169 = ©
    SPGM(htmlentities_deg, "&deg;"),        // 0xb0 = 176 = °
    SPGM(htmlentities_micro, "&plusmn;"),   // 0xb1 = 177 = ±
    SPGM(htmlentities_acute, "&acute;"),    // 0xb4 = 180 = ´
    SPGM(htmlentities_plusmn, "&micro;"),   // 0xb5 = 181 = µ
    // SPGM(htmlentities_, "&;"),
};


const char *__values_javascript_P[] PROGMEM = {
    SPGM(_escaped_apostrophe, "\\'"),           // '
    SPGM(_escaped_double_quote, "\\\""),        // "
    SPGM(_escaped_backslash, "\\\\"),           // \    Backslash
    SPGM(_escaped_backspace, "\\b"),            // \b	Backspace
    SPGM(_escaped_formfeed, "\\f"),             // \f	Form Feed
    SPGM(_escaped_newline, "\\n"),              // \n	New Line
    SPGM(_escaped_return, "\\r"),               // \r	Carriage Return
    SPGM(_escaped_htab, "\\t"),                 // \t	Horizontal Tabulator
    SPGM(_escaped_vtab, "\\v")                  // \v	Vertical Tabulator
};

int PrintHtmlEntities::getTranslatedSize_P(PGM_P str, Mode mode)
{
    if (!str) {
        return kNoTranslationRequired;
    }
    auto kv = __getKeysAndValues(mode);
    int requiredSize = 0;
    int len = 0;
    int8_t i;
    char ch;
    while((ch = pgm_read_byte(str)) != 0) {
        if ((i = __getKeyIndex_P(ch, kv.keys)) == -1) {
            requiredSize++;
        }
        else {
            requiredSize += strlen_P(kv.values[i]);
        }
        len++;
        str++;
    }
    return (len == requiredSize) ? kNoTranslationRequired : requiredSize;
}

bool PrintHtmlEntities::translateTo(const char *str, String &target, Mode mode, int requiredSize)
{
    if (!str) {
        return false;
    }
    if (requiredSize == kInvalidSize) {
        requiredSize = getTranslatedSize_P(str, mode);
    }
    if (requiredSize == kNoTranslationRequired || !target.reserve(requiredSize + target.length())) {
        return false;
    }
    auto kv = __getKeysAndValues(mode);
    char ch;
    while((ch = pgm_read_byte(str++)) != 0) {
        auto i = __getKeyIndex_P(ch, kv.keys);
        if (i == -1) {
            target += ch;
        }
        else {
            target += FPSTR(kv.values[i]);
        }
    }
    return true;
}

char *PrintHtmlEntities::translateTo(const char *str, Mode mode, int requiredSize)
{
    if (!str){
        return nullptr;
    }
    if (requiredSize == kInvalidSize) {
        requiredSize = getTranslatedSize_P(str, mode);
    }
    if (requiredSize == kNoTranslationRequired) {
        return nullptr;
    }
    auto target = reinterpret_cast<char *>(malloc(requiredSize + 1));
    if (target) {
        return nullptr;
    }
    auto ptr = target;
    auto kv = __getKeysAndValues(mode);
    char ch;
    while ((ch = pgm_read_byte(str++)) != 0) {
        auto i = __getKeyIndex_P(ch, kv.keys);
        if (i == -1) {
            *ptr++ = ch;
        }
        else {
            ptr += strlen(strcpy_P(ptr, kv.values[i]));

        }
    }
    *ptr = 0;
    return target;
}

size_t PrintHtmlEntities::printTo(Mode mode, const char *str, Print &output)
{
    if (!str) {
        return 0;
    }
    if (mode == Mode::RAW) {
        return output.print(FPSTR(str));
    }
    auto requiredSize = getTranslatedSize_P(str, mode);
    if (requiredSize == kNoTranslationRequired) {
        return output.print(FPSTR(str));
    }
    size_t written = 0;
    auto kv = __getKeysAndValues(mode);
    char ch;
    while((ch = pgm_read_byte(str)) != 0) {
        auto i = __getKeyIndex_P(ch, kv.keys);
        if (i == -1) {
            written += output.print(ch);
        }
        else {
            written += output.print(FPSTR(kv.values[i]));
        }
        str++;
    }
    return written;
}


size_t PrintHtmlEntities::translate(uint8_t data)
{
    if (_mode == Mode::RAW) {
        return writeRaw(data);
    }
    auto kv = __getKeysAndValues(_mode);
    auto i = __getKeyIndex_P((char)data, kv.keys);
    if (i != -1) {
        return _writeRawString(FPSTR(kv.values[i]));
    }
    if (data >= 0x80) {
        // sequence active?
        if (_utf8Count) {
            if ((data & 0b110000000) != 0b10000000) {
                // not a data byte
                __DBG_printf("invalid utf8 sequence count=%u data=0x%02x data=0x%06x", _utf8Count, data, _lastChars);
                _utf8Count = 0;
                return 0;
            }
            if (--_utf8Count == 0) {
                if (_lastChars & (1U << 23)) { // 3 bytes have been shifted
                    writeRaw(_lastChars >> 16);
                    writeRaw((_lastChars >> 8) & 0xff);
                    writeRaw(_lastChars & 0xff);
                    writeRaw(data);
                    return 4;
                }
                else if (_lastChars & (1U << 15)) {
                    writeRaw(_lastChars >> 8);
                    writeRaw(_lastChars & 0xff);
                    writeRaw(data);
                    return 3;
                }
                else if (_lastChars & (1U << 7)) {
                    writeRaw(_lastChars);
                    writeRaw(data);
                    return 2;
                }
                // invalid length, should be unreachable
                return 0;
            }
            else {
                // add data byte
                _lastChars <<= 8;
                _lastChars |= data;
                return 0;
            }
        }
        else if ((data & 0b11110000) == 0b11110000) {
            // length 4
            _utf8Count = 3;
            _lastChars = data;
            return 0;
        }
        else if ((data & 0b111100000) == 0b11100000) {
            // length 3
            _utf8Count = 2;
            _lastChars = data;
            return 0;
        }
        else if ((data & 0b111000000) == 0b11000000) {
            // length 2
            _utf8Count = 1; // 1 byte left
            _lastChars = data;
            return 0;
        }
        else {
            // data byte received but sequence not active
            __DBG_printf("invalid utf8 sequence count=%u data=0x%02x data=0x%06x", _utf8Count, data, _lastChars);
            _utf8Count = 0;
            return 0;
        }
    }
    else {
#if 1
        if (_utf8Count) {
            __DBG_printf("invalid utf8 sequence count=%u data=0x%02x last=0x%06x", _utf8Count, data, _lastChars);
        }
#endif
        _utf8Count = 0;
        switch (data) {
            case *HTML_TAG_S:
                return writeRaw('<');
            case *HTML_TAG_E:
                return writeRaw('>');
            case *HTML_AMP:
                return writeRaw('&');
            case *HTML_QUOTE:
                return writeRaw('"');
            case *HTML_EQUALS:
                return writeRaw('=');
            case *HTML_PCT:
                return writeRaw('%');
            case *HTML_APOS:
                return writeRaw('\'');
        }
    }
    return writeRaw(data);
}

PrintHtmlEntities::KeysValues PrintHtmlEntities::__getKeysAndValues(PrintHtmlEntities::Mode mode)
{
    if (mode == PrintHtmlEntities::Mode::JAVASCRIPT) {
        return KeysValues(__keys_javascript_P, __values_javascript_P);
    }
    else if (kAttributeOffset == 0) { // constexpr
        return KeysValues(__keys_attribute_all_P, __values_P);
    }
    else if (mode == PrintHtmlEntities::Mode::HTML) {
        return KeysValues(__keys_attribute_all_P, __values_P);
    }
    else if (mode == PrintHtmlEntities::Mode::ATTRIBUTE) {
        return KeysValues(&__keys_attribute_all_P[kAttributeOffset], &__values_P[kAttributeOffset]);
    }
    return KeysValues(__keys_attribute_all_P, __values_P);
}
