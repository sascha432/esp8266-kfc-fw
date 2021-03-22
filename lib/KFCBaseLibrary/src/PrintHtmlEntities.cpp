/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintHtmlEntities.h"
#include "PrintHtmlEntitiesString.h"

// copy src string into dst memcpy_strlen_P without terminating NUL byte and return copied bytes

//1600000: 361285 micros
static inline size_t memcpy_strlen_P(char *dst, PGM_P src)
{
    return strlen(strcpy_P(dst, src));

}

#if 0

//1600000: 640013 micros
static size_t memcpy_strlen_P_v2(char *dst, PGM_P src) {
    uint16_t count = 0;
    char ch;
    while ((ch = pgm_read_byte(src++)) != 0) {
        *dst++ = ch;
        count++;
    }
    return count;
}

//1600000: 621269 micros
static size_t memcpy_strlen_P_v3(char *dst, PGM_P src) {
    auto ptr = dst;
    char ch;
    while ((ch = pgm_read_byte(src++)) != 0) {
        *ptr++ = ch;
    }
    return (ptr - dst);
}

#endif

// index of the character must match the index of string in the values array
static const char __keys_attribute_all_P[] PROGMEM = { "'\"<>&" };

static const char __keys_javascript_P[] PROGMEM = { "'\"\\\b\f\n\r\t\v" };

#if 1
// do not translate ' inside html attributes
static constexpr size_t kAttributeOffset = 1;
#else
static constexpr size_t kAttributeOffset = 0;
#endif

// index of the character must match the index of string in the values array
static const char __keys_html_P[] PROGMEM =  { "'\"<>&%=?#" PRINTHTMLENTITIES_COPY PRINTHTMLENTITIES_DEGREE PRINTHTMLENTITIES_PLUSM PRINTHTMLENTITIES_ACUTE PRINTHTMLENTITIES_MICRO };

#ifndef _MSC_VER
static_assert(strncmp(__keys_attribute_all_P, __keys_html_P, strlen(__keys_attribute_all_P)) == 0, "invalid order");
#endif

// https://en.wikipedia.org/wiki/Windows-1252
static const char *__values_P[] PROGMEM = {
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

static const char *__values_javascript_P[] PROGMEM = {
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


static int8_t __getKeyIndex_P(char find, PGM_P keys)
{
    uint8_t count = 0;
    char ch;
    while((ch = pgm_read_byte(keys)) != 0) {
        if (ch == find) {
            return count;
        }
        count++;
        keys++;
    }
    return -1;
}

struct KeysValues {
    PGM_P keys;
    PGM_P *values;
    KeysValues(PGM_P _keys, PGM_P *_values) : keys(_keys), values(_values) {}
};

inline static KeysValues __getKeysAndValues(PrintHtmlEntities::Mode mode)
{
    if (mode == PrintHtmlEntities::Mode::JAVASCRIPT) {
        return KeysValues(__keys_javascript_P, __values_javascript_P);
    }
    else if (mode == PrintHtmlEntities::Mode::ATTRIBUTE) {
        return KeysValues(&__keys_attribute_all_P[kAttributeOffset], &__values_P[kAttributeOffset]);
    }
    return KeysValues(__keys_attribute_all_P, __values_P);
}

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
            ptr += memcpy_strlen_P(ptr, kv.values[i]);
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
        if ((data & 0b11110000) == 0b11110000) {
            // len 4
            _utf8Count = 3;
            _lastChars = data;
            return 0;
        }
        else if ((data & 0b111100000) == 0b11100000) {
            // len 3
            _utf8Count = 2;
            _lastChars = data;
            return 0;
        }
        else if ((data & 0b111000000) == 0b11000000) {
            // len 2
            _utf8Count = 1; // 1 byte left
            _lastChars = data;
            return 0;
        }
        else if ((data & 0b110000000) == 0b10000000) {
            // data byte
            if (_utf8Count) { // skip the data byte if the count is zero
                if (--_utf8Count == 0) {
                    if (_lastChars & (1U << 23)) { // 3 bytes have been shifted
                        writeRaw(_lastChars >> 16);
                        writeRaw((_lastChars >> 8) & 0xff);
                        writeRaw(_lastChars & 0xff);
                        writeRaw(data);
                        _utf8Count = 0;
                        return 4;
                    }
                    else if (_lastChars & (1U << 15)) {
                        writeRaw(_lastChars >> 8);
                        writeRaw(_lastChars & 0xff);
                        writeRaw(data);
                        _utf8Count = 0;
                        return 3;
                    }
                    else if (_lastChars & (1U << 7)) {
                        writeRaw(_lastChars);
                        writeRaw(data);
                        _utf8Count = 0;
                        return 2;
                    }
                }
                else {
                    _lastChars <<= 8;
                    _lastChars |= data;
                    return 0;
                }
            }
            __DBG_printf("invalid utf8 sequence count=%u data=0x%02x data=0x%06x", _utf8Count, data, _lastChars);
            return 0;
        }
        else {
            // invalid
            __DBG_printf("invalid utf8 sequence count=%u data=0x%02x data=0x%06x", _utf8Count, data, _lastChars);
            _utf8Count = 0;
            return 0;
        }
    }
    else {
        if (_utf8Count) {
            __DBG_printf("invalid utf8 sequence count=%u data=0x%02x last=0x%06x", _utf8Count, data, _lastChars);
        }
        _utf8Count = 0;
        switch (data) {
            case '\1':
                return writeRaw('<');
            case '\2':
                return writeRaw('>');
            case '\3':
                return writeRaw('&');
            case '\4':
                return writeRaw('"');
            case '\5':
                return writeRaw('=');
            case '\6':
                return writeRaw('%');
            case '\7':
                return writeRaw('\'');
        }
    }
    return writeRaw(data);
}
