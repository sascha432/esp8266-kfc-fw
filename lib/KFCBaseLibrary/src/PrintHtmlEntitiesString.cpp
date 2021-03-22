/**
* Author: sascha_lammers@gmx.de
*/

#include "PrintHtmlEntitiesString.h"

PrintHtmlEntitiesString::PrintHtmlEntitiesString() : PrintHtmlEntities(Mode::HTML)
{
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode) : PrintHtmlEntities(mode)
{
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const String &str)
{
    print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const __FlashStringHelper *str)
{
    print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const char *str)
{
    print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const String &str) : PrintHtmlEntities(mode)
{
    print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const __FlashStringHelper *str) : PrintHtmlEntities(mode)
{
    print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const char *str) : PrintHtmlEntities(mode)
{
    print(str);
}

size_t PrintHtmlEntitiesString::write(uint8_t data)
{
    return translate(data);
}

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
