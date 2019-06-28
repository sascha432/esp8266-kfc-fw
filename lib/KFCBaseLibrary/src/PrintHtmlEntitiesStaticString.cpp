/**
 * Author: sascha_lammers@gmx.de
 */

#include "PrintHtmlEntitiesStaticString.h"

PrintHtmlEntitiesStaticString::PrintHtmlEntitiesStaticString(char *buffer, size_t size, const String &str) : PrintStaticString(buffer, size), PrintHtmlEntities() {
    print(str);
}

PrintHtmlEntitiesStaticString::PrintHtmlEntitiesStaticString(char *buffer, size_t size, const __FlashStringHelper *str) : PrintStaticString(buffer, size), PrintHtmlEntities() {
    print(str);
}

PrintHtmlEntitiesStaticString::PrintHtmlEntitiesStaticString(char *buffer, size_t size, const char *str) : PrintStaticString(buffer, size), PrintHtmlEntities() {
    print(str);
}

PrintHtmlEntitiesStaticString::PrintHtmlEntitiesStaticString(char *buffer, size_t size) : PrintStaticString(buffer, size), PrintHtmlEntities() {
}

size_t PrintHtmlEntitiesStaticString::write(uint8_t data) {
    return translate(data);
}

size_t PrintHtmlEntitiesStaticString::write(const uint8_t *buffer, size_t size) {
    return translate(buffer, size);
}

size_t PrintHtmlEntitiesStaticString::_write(uint8_t data) {
    return PrintStaticString::write(data);
}
