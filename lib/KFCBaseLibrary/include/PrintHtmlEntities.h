/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

// \1 and \2 are changed to < and > to preserve it, otherwise it gets replaced by &lt; and &gt;
#define HTML_TAG_S "\1"
#define HTML_TAG_E "\2"
#define HTML_S HTML_OPEN_TAG
#define HTML_E HTML_CLOSE_TAG
#define HTML_OPEN_TAG(tag) HTML_TAG_S #tag HTML_TAG_E
#define HTML_CLOSE_TAG(tag) HTML_TAG_S "/" #tag HTML_TAG_E
#define HTML_NEW_2COL_ROW HTML_E(td) HTML_E(tr) HTML_S(tr) HTML_S(td) "&nbsp;" HTML_E(td) HTML_S(td)

class PrintHtmlEntities {
public:
    PrintHtmlEntities();

    size_t __write(uint8_t data);

    size_t translate(uint8_t data);
    size_t translate(const uint8_t *buffer, size_t size);

    size_t _writeString(const __FlashStringHelper *str);

    virtual size_t _write(uint8_t data) = 0;
};
