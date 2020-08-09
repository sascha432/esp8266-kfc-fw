/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "PrintString.h"
#include "PrintHtmlEntities.h"

class PrintHtmlEntitiesString : public PrintString, public PrintHtmlEntities {
public:
    PrintHtmlEntitiesString() {}
    PrintHtmlEntitiesString(const String &str);
    PrintHtmlEntitiesString(const __FlashStringHelper *str);
    PrintHtmlEntitiesString(const char *str);
    PrintHtmlEntitiesString(Mode mode, const String &str);
    PrintHtmlEntitiesString(Mode mode, const __FlashStringHelper *str);
    PrintHtmlEntitiesString(Mode mode, const char *str);

    virtual size_t write(uint8_t data) override;
    virtual size_t writeRaw(uint8_t data) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    size_t printRaw(const String &str) {
        return printRaw(FPSTR(str.c_str()));
    }
    size_t printRaw(const char *str) {
        return printRaw(FPSTR(str));
    }
    size_t printRaw(const __FlashStringHelper *str) {
        if (str) {
            return _writeRawString(str);
        }
        return 0;
    }
};
