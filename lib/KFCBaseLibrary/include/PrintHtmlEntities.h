/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#define HTML_TAG_S  "\x01"
#define HTML_TAG_E  "\x02"
#define HTML_AMP    "\x03"
#define HTML_SPACE  "\x03nbsp;"
#define HTML_DEG    "\003deg;"
#define HTML_QUOTE  "\x04"
#define HTML_EQUALS "\x05"
#define HTML_S HTML_OPEN_TAG
#define HTML_E HTML_CLOSE_TAG
#define HTML_OPEN_TAG(tag) HTML_TAG_S #tag HTML_TAG_E
#define HTML_CLOSE_TAG(tag) HTML_TAG_S "/" #tag HTML_TAG_E
#define HTML_NEW_2COL_ROW HTML_E(td) HTML_E(tr) HTML_S(tr) HTML_S(td) HTML_SPACE HTML_E(td) HTML_S(td)

class PrintHtmlEntities {
public:
    PrintHtmlEntities();

    size_t __write(uint8_t data);

    size_t translate(uint8_t data);
    size_t translate(const uint8_t *buffer, size_t size);

    size_t _writeString(const __FlashStringHelper *str);

    virtual size_t _write(uint8_t data) = 0;
};
