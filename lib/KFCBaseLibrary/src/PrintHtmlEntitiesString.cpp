/**
* Author: sascha_lammers@gmx.de
*/

#include "PrintHtmlEntitiesString.h"

PrintHtmlEntitiesString::PrintHtmlEntitiesString()
{
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode) {
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const String &str)
{
    PrintString::print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const __FlashStringHelper *str)
{
    PrintString::print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const char *str)
{
    PrintString::print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const String &str) : PrintHtmlEntities(mode)
{
    PrintString::print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const __FlashStringHelper *str) : PrintHtmlEntities(mode)
{
    PrintString::print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(Mode mode, const char *str) : PrintHtmlEntities(mode)
{
    PrintString::print(str);
}

size_t PrintHtmlEntitiesString::write(uint8_t data)
{
    return translate(data);
}

size_t PrintHtmlEntitiesString::writeRaw(uint8_t data)
{
    return PrintString::write(data);
}

size_t PrintHtmlEntitiesString::write(const uint8_t *buffer, size_t size)
{
    if (_mode == Mode::RAW) {
        return PrintString::write(buffer, size);
    }
    return translate(buffer, size);
}
