/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "PrintString.h"
#include "PrintHtmlEntities.h"

class PrintHtmlEntitiesString : public PrintString, public PrintHtmlEntities {
public:
    PrintHtmlEntitiesString(const String &str);
    PrintHtmlEntitiesString(const __FlashStringHelper *str);
    PrintHtmlEntitiesString(const char *str);
    PrintHtmlEntitiesString();

    virtual size_t write(uint8_t data) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;
    virtual size_t _write(uint8_t data) override;
};
