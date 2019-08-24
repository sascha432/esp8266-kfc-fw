/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonNumber.h"
#include "JsonVar.h"
#include "JsonTools.h"

bool JsonNumber::validate() {
    auto strPtr = getPtr();
    char *ptr = (char *)strPtr;
    if (isProgMem()) {
        auto len = length();
        ptr = (char *)malloc(len + 1);
        strncpy_P(ptr, strPtr, len)[len] = 0;
    }
    auto valid = JsonVar::getNumberType(ptr) != JsonVar::INVALID;
    if (ptr != strPtr) {
        free(ptr);
    }
    if (!valid) {
        invalidate();
    }
    return valid;
}

void JsonNumber::invalidate() {
    _setType(STORED);
    strcpy_P(_raw, SPGM(null));
}
