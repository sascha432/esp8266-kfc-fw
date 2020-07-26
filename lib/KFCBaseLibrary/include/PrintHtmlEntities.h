/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

// NOTE: any string should be UTF-8
// check the encoding of your files that contain any strings
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

class PrintHtmlEntities {
public:
    enum class Mode {
        HTML,
        ATTRIBUTE,
        RAW,
    };

public:
    PrintHtmlEntities();

    // translate HTML_* and use writeRaw() to output it
    size_t translate(uint8_t data);
    // translate entire buffer
    // following utf8 characters are translated to html entities: ©°±´µ
    size_t translate(const uint8_t *buffer, size_t size);

    // write data and encode entities and html escape characters
    virtual size_t write(uint8_t data) = 0;
    // write data without any encoding
    virtual size_t writeRaw(uint8_t data) = 0;
    // Mode::RAW = do not encode anything
    // Mode::HTML = encode html entities for html output
    // Mode::ATTRIBUTE = encode string for the use as attribute value
    virtual void setMode(Mode mode);
    virtual Mode getMode() const;

    // methods to encode existing strings

    static constexpr int kNoTranslationRequired = -1;
    static constexpr int kInvalidSize = -2;

    // returns the length of the new string or -1 if it does not require any translation
    static int getTranslatedSize(const char *str, bool attribute);
    // the translated string is appended to target unless the function returns false, which means
    // the string does not require any translation (or allocating the memory failed)
    // if getTranslatedSize() was used before, the value should be passed as requiredSize
    // attribute=true encodes as value for html attributes, false encodes for html
    static bool translateTo(const char *str, String &target, bool attribute, int requiredSize = kInvalidSize);
    // returns string, avoid using it with huge strings
    static String getTranslatedTo(const String &str, bool attribute);

    // access to the transaction table
    static const __FlashBufferHelper *getAttributeKeys();
    static const __FlashBufferHelper *getHtmlKeys();
    static const __FlashBufferHelper **getTranslationTable();

private:
    size_t _writeRawString(const __FlashStringHelper *str);

    Mode _mode;
};
