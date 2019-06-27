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
        case '%':
            return _writeString(F("&percnt;"));
        case '<':
            return _writeString(F("&lt;"));
        case '>':
            return _writeString(F("&gt;"));
        case '\1':
            return __write('<');
        case '\2':
            return __write('>');
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
	size_t count = strlen_P(ptr);
	size_t written = 0;
	while (count--) {
		written += __write(pgm_read_byte(ptr++));
	}
	return written;
}
