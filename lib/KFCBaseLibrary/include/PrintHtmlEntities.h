/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

// it is recommended to use PRINTHTMLENTITIES_* for supported entities

#define HTML_TAG_S  "\x01"
#define HTML_TAG_E  "\x02"
#define HTML_AMP    "\x03"
#define HTML_SPACE  "\x03nbsp;"
#define HTML_QUOTE  "\x04"
#define HTML_EQUALS "\x05"
#define HTML_PCT    "\x06"
#define HTML_APOS   "\x07"
#define HTML_S                      HTML_OPEN_TAG
#define HTML_E                      HTML_CLOSE_TAG
#define HTML_OPEN_TAG(tag)          HTML_TAG_S #tag HTML_TAG_E
#define HTML_CLOSE_TAG(tag)         HTML_TAG_S "/" #tag HTML_TAG_E
#define HTML_SA(tag, attr)          HTML_TAG_S ___STRINGIFY(tag) attr HTML_TAG_E
#define HTML_A(key, value)          " " key HTML_EQUALS HTML_QUOTE value HTML_QUOTE

#define PRINTHTMLENTITIES_COPY      "\xa9"  // 0xa9 = 169 = ©
#define PRINTHTMLENTITIES_DEGREE    "\xb0"  // 0xb0 = 176 = °
#define PRINTHTMLENTITIES_PLUSM     "\xb1"  // 0xb1 = 177 = ±
#define PRINTHTMLENTITIES_ACUTE     "\xb4"  // 0xb4 = 180 = ´
#define PRINTHTMLENTITIES_MICRO     "\xb5"  // 0xb5 = 181 = µ

class PrintHtmlEntitiesString;

class PrintHtmlEntities {
public:
    enum class Mode : uint8_t {
        HTML,
        ATTRIBUTE,
        RAW,
        JAVASCRIPT
    };

public:
    PrintHtmlEntities();
    PrintHtmlEntities(Mode mode);

    // translate HTML_* and use writeRaw() to output it
    size_t translate(uint8_t data);
    // translate entire buffer
    size_t translate(const uint8_t *buffer, size_t size);

    // write data and encode entities and html escape characters
    virtual size_t write(uint8_t data) = 0;
    // write data without any encoding
    virtual size_t writeRaw(uint8_t data) = 0;
    // Mode::RAW = do not encode anything
    // Mode::HTML = encode html entities for html output
    // Mode::ATTRIBUTE = encode string for the use as attribute value
    virtual Mode setMode(Mode mode); // returns previous mode
    virtual Mode getMode() const;

    // methods to encode existing strings

    static constexpr int kNoTranslationRequired = -1;
    static constexpr int kInvalidSize = -2;

    // returns the length of the new string or -1 if it does not require any translation
    static int getTranslatedSize_P(PGM_P str, Mode mode);
    // append the translated string to target unless there isn't any translation
    // required or the memory allocation failed
    // if getTranslatedSize() was used before, the value should be passed as requiredSize
    //
    // attribute=true encode as value for html attributes
    // attributes=false encode as html
    //
    // NOTES:
    //  - return value false means that target was not modified
    //  - str is PROGMEM safe
    static bool translateTo(const char *str, String &target, Mode mode, int requiredSize = kInvalidSize);

    // returns pointer to allocated str or nullptr if no translation was required
    static char *translateTo(const char *str, Mode mode, int requiredSize = kInvalidSize);

    static size_t printTo(Mode mode, const char *str, Print &output);
    static size_t printTo(Mode mode, const String &str, Print &output);
    static size_t printTo(Mode mode, const __FlashStringHelper *str, Print &output);

    // returns string, avoid using it with huge strings
    static String getTranslatedTo(const String &str, Mode mode);

protected:
    size_t _writeRawString(const __FlashStringHelper *str);

private:
    Mode _mode;
    uint8_t _lastChar;
};

inline __attribute__((__always_inline__))
PrintHtmlEntities::PrintHtmlEntities() :
    _mode(Mode::HTML), _lastChar(0)
{
}

inline __attribute__((__always_inline__))
PrintHtmlEntities::PrintHtmlEntities(Mode mode) :
    _mode(mode)
{
}

inline __attribute__((__always_inline__))
PrintHtmlEntities::Mode PrintHtmlEntities::setMode(Mode mode)
{
    Mode lastMode = _mode;
    _mode = mode;
    _lastChar = 0;
    return lastMode;
}

inline __attribute__((__always_inline__))
PrintHtmlEntities::Mode PrintHtmlEntities::getMode() const
{
    return _mode;
}

inline __attribute__((__always_inline__))
size_t PrintHtmlEntities::printTo(Mode mode, const String &str, Print &output)
{
    return printTo(mode, str.c_str(), output);
}

inline __attribute__((__always_inline__))
size_t PrintHtmlEntities::printTo(Mode mode, const __FlashStringHelper *str, Print &output)
{
    return printTo(mode, reinterpret_cast<PGM_P>(str), output);
}

inline __attribute__((__always_inline__))
String PrintHtmlEntities::getTranslatedTo(const String &str, Mode mode)
{
    String tmp;
    if (translateTo(str.c_str(), tmp, mode)) {
        return tmp;
    }
    return str;
}
