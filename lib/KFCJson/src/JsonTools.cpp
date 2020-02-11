/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonTools.h"
#include "JsonVar.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// #if !HAVE_PROGMEM_DATA

PROGMEM_STRING_DEF(true, "true");
PROGMEM_STRING_DEF(false, "false");
PROGMEM_STRING_DEF(null, "null");

// #endif

const __FlashStringHelper *JsonTools::boolToString(bool value) {
    return value ? FSPGM(true) : FSPGM(false);
}

size_t JsonTools::lengthEscaped(const JsonString & value) {
    if (value.isProgMem()) {
        return lengthEscaped(value.getPtr(), value.length(), true);
    }
    else {
        return lengthEscaped(value.getPtr(), value.length());
    }
}

static const char *charatersToEscape = "\b\f\t\r\n\\\"";

size_t JsonTools::lengthEscaped(const char *value, size_t length, bool isProgMem) {
    if (!length) {
        return 0;
    }
    auto ptr = value;
#if !defined(ESP32)
    if (isProgMem) {
        char ch;
        while ((ch = (char)pgm_read_byte(ptr++)) != 0) {
            if (strchr(charatersToEscape, ch)) {
                length++;
            }
        }
    } else
#endif
    {
        ptr = strpbrk(ptr, charatersToEscape);
        while (ptr) {
            length++;
            ptr = strpbrk(ptr + 1, charatersToEscape);
        }
    }
    return length;
}

size_t JsonTools::printToEscaped(Print & output, const JsonString & value) {
    if (value.isProgMem()) {
        return printToEscaped(output, value.getPtr(), value.length(), true);
    }
    else {
        return printToEscaped(output, value.getPtr(), value.length());
    }
}

size_t JsonTools::printToEscaped(Print &output, const char *value, size_t length, bool isProgMem) {
    if (!length) {
        return 0;
    }
    size_t _length = 0;
    const char *sptr = value;
    //TODO test if ESP8266 newlibc strpbrk can handle progmem strings now
#if !defined(ESP32)
    if (isProgMem) {

        // no strpbrk_P, just copy byte by byte
        char ch;
        while ((ch = static_cast<char>(pgm_read_byte(sptr++)))) {
            if (strchr(charatersToEscape, ch)) {
                switch (printEscaped(output, ch)) {
                case 0:
                    return _length;
                case 1:
                    return ++_length;
                }
                _length++;
            }
            else {
                if (!output.write(ch)) {
                    return _length;
                }
            }
            _length++;
        }

    } else
#endif
    {
        const char *ptr = strpbrk(sptr, charatersToEscape);
        if (ptr) {
            do {
                // copy until character to escape
                size_t len = ptr - sptr;
                size_t written = output.write(reinterpret_cast<const uint8_t *>(sptr), len);
                _length += written;
                if (written != len) {
                    return _length;
                }
                sptr = ptr;
                // escape single character
                switch (printEscaped(output, *sptr++)) {
                case 0:
                    return _length;
                case 1:
                    return ++_length;
                }
                _length += 2;
                // and next please
                ptr = strpbrk(sptr, charatersToEscape);
            } while(ptr);

            // copy rest of the string
            size_t len = value - sptr + length;
            size_t written = output.write(reinterpret_cast<const uint8_t *>(sptr), len);
            _length += written;
        } else {
            size_t written = output.write(reinterpret_cast<const uint8_t *>(sptr), length);
            _length += written;
        }
    }
    return _length;
}
