/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "PrintString.h"
#include "PrintHtmlEntities.h"

class PrintHtmlEntitiesString : public PrintString, public PrintHtmlEntities {
public:
    PrintHtmlEntitiesString();
    PrintHtmlEntitiesString(Mode mode);
    PrintHtmlEntitiesString(const String &str);
    PrintHtmlEntitiesString(const __FlashStringHelper *str);
    PrintHtmlEntitiesString(const char *str);
    PrintHtmlEntitiesString(Mode mode, const String &str);
    PrintHtmlEntitiesString(Mode mode, const __FlashStringHelper *str);
    PrintHtmlEntitiesString(Mode mode, const char *str);

    virtual size_t write(uint8_t data) override;
    virtual size_t writeRaw(uint8_t data) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    size_t printRaw(const String &str);
    size_t printRaw(const char *str);
    size_t printRaw(const __FlashStringHelper *str);
};

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString() : PrintHtmlEntities(Mode::HTML)
{
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode) : PrintHtmlEntities(mode)
{
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(const String &str)
{
    print(str);
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(const __FlashStringHelper *str)
{
    print(str);
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(const char *str)
{
    print(str);
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const String &str) : PrintHtmlEntities(mode)
{
    print(str);
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const __FlashStringHelper *str) : PrintHtmlEntities(mode)
{
    print(str);
}

inline PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const char *str) : PrintHtmlEntities(mode)
{
    print(str);
}

inline __attribute__((__always_inline__))
size_t PrintHtmlEntitiesString::write(uint8_t data)
{
    return translate(data);
}

inline __attribute__((__always_inline__))
size_t PrintHtmlEntitiesString::writeRaw(uint8_t data)
{
    return PrintString::write(data);
}

inline size_t PrintHtmlEntitiesString::write(const uint8_t *buffer, size_t size)
{
    if (getMode() == Mode::RAW) {
        return PrintString::write(buffer, size);
    }
    return translate(buffer, size);
}

inline size_t PrintHtmlEntitiesString::printRaw(const String &str)
{
    return printRaw(FPSTR(str.c_str()));
}

inline size_t PrintHtmlEntitiesString::printRaw(const char *str)
{
    return printRaw(FPSTR(str));
}

inline size_t PrintHtmlEntitiesString::printRaw(const __FlashStringHelper *str)
{
    if (str) {
        return _writeRawString(str);
    }
    return 0;
}
