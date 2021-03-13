#if defined(_WIN32) || defined(_WIN64)

#include <Arduino_compat.h>
#include "StreamString.h"

size_t StreamString::write(const uint8_t *data, size_t size) {
    if(size && data) {
        this->concat((const char *)data, size);
        return size;
    }
    return 0;
}

size_t StreamString::write(uint8_t data) {
    return concat((char) data);
}

int StreamString::available() {
    return length();
}

int StreamString::read() {
    if(length()) {
        char c = charAt(0);
        remove(0, 1);
        return (uint8_t)c;

    }
    return -1;
}

int StreamString::peek() {
    if(length()) {
        char c = charAt(0);
        return (uint8_t)c;
    }
    return -1;
}

void StreamString::flush() {
}

#endif
