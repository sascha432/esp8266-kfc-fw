#include "PrintHtmlEntitiesString.h"

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const String & str) : PrintString(), PrintHtmlEntities() {
	print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const __FlashStringHelper * str) : PrintString(), PrintHtmlEntities() {
	print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString(const char * str) : PrintString(), PrintHtmlEntities() {
	print(str);
}

PrintHtmlEntitiesString::PrintHtmlEntitiesString() : PrintString(), PrintHtmlEntities() {
}

size_t PrintHtmlEntitiesString::write(uint8_t data) {
	return translate(data);
}

size_t PrintHtmlEntitiesString::write(const uint8_t * buffer, size_t size) {
	return translate(buffer, size);
}

size_t PrintHtmlEntitiesString::_write(uint8_t data) {
	return PrintString::write(data);
}
