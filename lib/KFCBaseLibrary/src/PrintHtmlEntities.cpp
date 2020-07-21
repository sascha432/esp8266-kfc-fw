/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintHtmlEntities.h"

// TODO proper UTF8 handling
// utf8 is just passed except for &deg; &copy; &micro; and &acute;

PrintHtmlEntities::PrintHtmlEntities() : _raw(false) {
}

void PrintHtmlEntities::setRawOutput(bool raw)
{
    _raw = raw;
}

size_t PrintHtmlEntities::translate(uint8_t data)
{
    if (!_raw) {
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
                // case 39:
            //     break;
            // default:
            //     if (data >= 0xa0 && data <= 0xbf) {
            //         uint8_t count = writeRaw('&');
            //         count += writeRaw('#');
            //         count += writeRaw(xxx((data & 0xf0) >> 4));
            //         count += writeRaw(xxx(data & 0xf));
            //         return count + writeRaw(';');
            //     }
            //     break;
        }
    }
    return writeRaw(data);
}

size_t PrintHtmlEntities::translate(const uint8_t * buffer, size_t size) {
    size_t written = 0;
    while (size--) {
        if (*buffer == 0xc2 && size) {
            switch(*(buffer + 1)) {
                case 0xb0:
                case 0xa9:
                case 0xb5:
                case 0xb4:
                    // utf8 encoded, we cannot just replace it
                    size--;
                    buffer++;
                    break;
            }
        }
        translate(*buffer++);
        written++;
    }
    return written;
}

size_t PrintHtmlEntities::_writeString(const __FlashStringHelper * str) {
    PGM_P ptr = RFPSTR(str);
    size_t written = 0;
    uint8_t ch;
    while ((ch = pgm_read_byte(ptr++))) {
        written += writeRaw(ch);
    }
    return written;
}
