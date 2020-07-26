/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintHtmlEntities.h"

PrintHtmlEntities::PrintHtmlEntities() : _mode(Mode::HTML)
{
}

void PrintHtmlEntities::setMode(Mode mode)
{
    _mode = mode;
}

PrintHtmlEntities::Mode PrintHtmlEntities::getMode() const
{
    return _mode;
}


// index of the character must match index of string in array
static const char __keys_attribute_all[] PROGMEM = { "'\"<>&" };
#if 1
// do not translate ' inside html attributes
static constexpr const char *__keys_attribute_P = &__keys_attribute_all[1];
#else
static constexpr const char *__keys_attribute_P = &__keys_attribute_all[0];
#endif

static const char __keys_html_P[] PROGMEM =  { "'\"<>&%=?#" PRINTHTMLENTITIES_COPY PRINTHTMLENTITIES_DEGREE PRINTHTMLENTITIES_PLUSM PRINTHTMLENTITIES_ACUTE PRINTHTMLENTITIES_MICRO };
static const char *__keys_utf8_P = StringConstExpr::strchr(__keys_html_P, PRINTHTMLENTITIES_COPY[0]);

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

int PrintHtmlEntities::getTranslatedSize(const char *str, bool attribute)
{
    auto keys = attribute ? __keys_attribute_P : __keys_html_P;
    int requiredSize = 0;
    int len = 0;
    int8_t i;
    while(*str) {
        if ((i = __getKeyIndex_P(*str, keys)) == -1) {
            requiredSize++;
        }
        else {
            requiredSize += strlen_P(__values_P[i]);
        }
        len++;
        str++;
    }
    return (len == requiredSize) ? kNoTranslationRequired : requiredSize;
}

#include "PrintString.h"//TODO remove
#ifndef _MSC_VER
#warning TODO remove
#endif

bool PrintHtmlEntities::translateTo(const char *cStr, String &target, bool attribute, int requiredSize)
{
    if (requiredSize == kInvalidSize) {
        requiredSize = getTranslatedSize(cStr, attribute);
    }
    if (requiredSize != kNoTranslationRequired) {
        if (target.reserve(requiredSize + target.length())) {
            auto keys = attribute ? __keys_attribute_P : __keys_html_P;
            auto tmp = PrintString(F("in='%s' target='%s'"), cStr, target.c_str());//TODO remove
            while(*cStr) {
                auto i = __getKeyIndex_P(*cStr, keys);
                if (i == -1) {
                    target += *cStr;
                }
                else {
                    target += FPSTR(__values_P[i]);
                }
                cStr++;
            }
            __DBG_printf("%s output='%s'", tmp.c_str(), target.c_str());//TODO remove
            return true;
        }
    }
    return false;
}

String PrintHtmlEntities::getTranslatedTo(const String &str, bool attribute)
{
    String tmp;
    translateTo(str.c_str(), tmp, attribute);
    return tmp;
}

size_t PrintHtmlEntities::translate(uint8_t data)
{
    if (_mode != Mode::RAW) {
        auto i = __getKeyIndex_P((char)data, _mode == Mode::HTML ? __keys_html_P : __keys_attribute_P);
        if (i != -1) {
            return _writeRawString(FPSTR(__values_P[i]));
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

size_t PrintHtmlEntities::translate(const uint8_t * buffer, size_t size)
{
    size_t written = 0;
    while (size--) {
        if (*buffer == 0xc2 && size >= 1) { // UTF8 encoded character
            auto i = __getKeyIndex_P(*(buffer + 1), _mode == Mode::HTML ? __keys_html_P : __keys_attribute_P);
            if (i != -1) { // we support only a few entities
                written += _writeRawString(FPSTR(__values_P[i]));
                buffer += 2;
            }
            else {
                // output as UTF-8, html charset is utf-8
                written += writeRaw(*buffer++);
                written += writeRaw(*buffer++);
            }
            size--;
        }
        else {
            written += translate(*buffer++);
        }
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


const __FlashBufferHelper *PrintHtmlEntities::getAttributeKeys()
{
    return reinterpret_cast<const __FlashBufferHelper *>(__keys_attribute_P);
}

const __FlashBufferHelper *PrintHtmlEntities::getHtmlKeys()
{
    return reinterpret_cast<const __FlashBufferHelper *>(__keys_html_P);
}

const __FlashBufferHelper **PrintHtmlEntities::getTranslationTable()
{
    return reinterpret_cast<const __FlashBufferHelper **>(__values_P);
}
