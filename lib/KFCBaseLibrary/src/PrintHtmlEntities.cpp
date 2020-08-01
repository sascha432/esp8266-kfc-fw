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

PrintHtmlEntities::PrintHtmlEntities() : _mode(Mode::HTML)
{
}

PrintHtmlEntities::Mode PrintHtmlEntities::setMode(Mode mode)
{
    Mode lastMode = _mode;
    _mode = mode;
    return lastMode;
}

PrintHtmlEntities::Mode PrintHtmlEntities::getMode() const
{
    return _mode;
}

// index of the character must match the index of string in the values array
static const char __keys_attribute_all_P[] PROGMEM = { "'\"<>&" };

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

static void __getKeysAndValues(PGM_P *keys, PGM_P **values, bool attribute)
{
    if (attribute) {
        *keys = &__keys_attribute_all_P[kAttributeOffset];
        *values = &__values_P[kAttributeOffset];
    }
    else {
        *keys = __keys_attribute_all_P;
        *values = __values_P;
    }
}

int PrintHtmlEntities::getTranslatedSize_P(PGM_P str, bool attribute)
{
    if (!str) {
        return kNoTranslationRequired;
    }
    PGM_P keys;
    PGM_P *values;
    __getKeysAndValues(&keys, &values, attribute);
    int requiredSize = 0;
    int len = 0;
    int8_t i;
    char ch;
    while((ch = pgm_read_byte(str)) != 0) {
        if ((i = __getKeyIndex_P(ch, keys)) == -1) {
            requiredSize++;
        }
        else {
            requiredSize += strlen_P(values[i]);
        }
        len++;
        str++;
    }
    return (len == requiredSize) ? kNoTranslationRequired : requiredSize;
}

bool PrintHtmlEntities::translateTo(const char *str, String &target, bool attribute, int requiredSize)
{
    if (!str) {
        return false;
    }
    if (requiredSize == kInvalidSize) {
        requiredSize = getTranslatedSize_P(str, attribute);
    }
    if (requiredSize != kNoTranslationRequired) {
        if (target.reserve(requiredSize + target.length())) {
            PGM_P keys;
            PGM_P *values;
            __getKeysAndValues(&keys, &values, attribute);
            char ch;
            while((ch = pgm_read_byte(str++)) != 0) {
                auto i = __getKeyIndex_P(ch, keys);
                if (i == -1) {
                    target += ch;
                }
                else {
                    target += FPSTR(values[i]);
                }
            }
            return true;
        }
    }
    return false;
}

char *PrintHtmlEntities::translateTo(const char *str, bool attribute, int requiredSize)
{
    char *target = nullptr;
    if (str) {
        if (requiredSize == kInvalidSize) {
            requiredSize = getTranslatedSize_P(str, attribute);
        }
        if (requiredSize != kNoTranslationRequired) {
            target = reinterpret_cast<char *>(malloc(requiredSize + 1));
            if (target) {
                auto ptr = target;
                PGM_P keys;
                PGM_P *values;
                __getKeysAndValues(&keys, &values, attribute);
                char ch;
                while ((ch = pgm_read_byte(str++)) != 0) {
                    auto i = __getKeyIndex_P(ch, keys);
                    if (i == -1) {
                        *ptr++ = ch;
                    }
                    else {
                        ptr += memcpy_strlen_P(ptr, values[i]);
                    }
                }
                *ptr = 0;
            }
        }
    }
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
    auto attribute = (mode == Mode::ATTRIBUTE);
    auto requiredSize = getTranslatedSize_P(str, attribute);
    if (requiredSize == kNoTranslationRequired) {
        return output.print(FPSTR(str));
    }
    size_t written = 0;
    PGM_P keys;
    PGM_P *values;
    __getKeysAndValues(&keys, &values, attribute);
    char ch;
    while((ch = pgm_read_byte(str)) != 0) {
        auto i = __getKeyIndex_P(ch, keys);
        if (i == -1) {
            written += output.print(ch);
        }
        else {
            written += output.print(FPSTR(values[i]));
        }
        str++;
    }
    return written;
}

size_t PrintHtmlEntities::printTo(Mode mode, const char *str, PrintHtmlEntitiesString &output)
{
    if (mode == Mode::RAW) {
        return output.printRaw(str);
    }
    auto prevMode = output.setMode(mode);
    size_t written = output.print(str);
    output.setMode(prevMode);
    return written;
}

size_t PrintHtmlEntities::printTo(Mode mode, const __FlashStringHelper *str, PrintHtmlEntitiesString &output)
{
    if (mode == Mode::RAW) {
        return output.printRaw(str);
    }
    auto prevMode = output.setMode(mode);
    size_t written = output.print(str);
    output.setMode(prevMode);
    return written;
}

String PrintHtmlEntities::getTranslatedTo(const String &str, bool attribute)
{
    String tmp;
    if (translateTo(str.c_str(), tmp, attribute)) {
        return tmp;
    }
    return str;
}

size_t PrintHtmlEntities::translate(uint8_t data)
{
    if (_mode != Mode::RAW) {
        PGM_P keys;
        PGM_P *values;
        __getKeysAndValues(&keys, &values, _mode == Mode::ATTRIBUTE);
        auto i = __getKeyIndex_P((char)data, keys);
        if (i != -1) {
            return _writeRawString(FPSTR(values[i]));
        }
        else {
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
    }
    return writeRaw(data);
}

size_t PrintHtmlEntities::translate(const uint8_t *buffer, size_t size)
{
    size_t written = 0;
    while (size--) {
        written += translate(*buffer++);
    }
    return written;
}

size_t PrintHtmlEntities::_writeRawString(const __FlashStringHelper *str)
{
    PGM_P ptr = RFPSTR(str);
    size_t written = 0;
    uint8_t ch;
    while ((ch = pgm_read_byte(ptr++)) != 0) {
        written += writeRaw(ch);
    }
    return written;
}
