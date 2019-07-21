/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintHtmlEntities.h"

PrintHtmlEntities::PrintHtmlEntities() {
}

size_t PrintHtmlEntities::__write(uint8_t data) {
    return _write(data);
}

size_t PrintHtmlEntities::translate(uint8_t data) {
    switch (data) {
        case 0xb0:
            return _writeString(F("&deg;"));
        case 0xa9:
            return _writeString(F("&copy;"));
        case 0xb5:
            return _writeString(F("&micro;"));
        case 0xb4:
            return _writeString(F("&acute;"));
        case '=':
            return _writeString(F("&equals;"));
        case '?':
            return _writeString(F("&quest;"));
        case '#':
            return _writeString(F("&num;"));
        case '%':
            return _writeString(F("&percnt;"));
        case '"':
            return _writeString(F("&quot;"));
        case '&':
            return _writeString(F("&amp;"));
        case '<':
            return _writeString(F("&lt;"));
        case '>':
            return _writeString(F("&gt;"));
        case '\1':
            return __write('<');
        case '\2':
            return __write('>');
        case '\3':
            return __write('&');
        case 39:
        default:
            if (data >= 0xa0 && data <= 0xbf) {
                uint8_t count = __write('&');
                count += __write('#');
                count += __write(nibble2hex((data & 0xf0) >> 4));
                count += __write(nibble2hex(data & 0xf));
                return count + __write(';');
            }
    }
    return __write(data);
}

size_t PrintHtmlEntities::translate(const uint8_t * buffer, size_t size) {
    size_t written = 0;
    while (size--) {
        written = translate(*buffer++);
    }
    return written;
}

size_t PrintHtmlEntities::_writeString(const __FlashStringHelper * str) {
    PGM_P ptr = reinterpret_cast<PGM_P>(str);
    size_t written = 0;
    uint8_t ch;
    while ((ch = pgm_read_byte(ptr++))) {
        written += __write(ch);
    }
    return written;
}
