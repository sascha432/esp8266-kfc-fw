/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "PrintStaticString.h"
#include "PrintHtmlEntities.h"

class PrintHtmlEntitiesStaticString : public PrintStaticString, public PrintHtmlEntities {
public:
    PrintHtmlEntitiesStaticString(char *buffer, size_t size, const String &str);
    PrintHtmlEntitiesStaticString(char *buffer, size_t size, const __FlashStringHelper *str);
    PrintHtmlEntitiesStaticString(char *buffer, size_t size, const char *str);
    PrintHtmlEntitiesStaticString(char *buffer, size_t size);

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual size_t _write(uint8_t data) override;
};
