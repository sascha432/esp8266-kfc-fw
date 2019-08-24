/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonObject.h"
#include "JsonArray.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

JsonArray & JsonObjectMethods::addArray(const JsonString & name, size_t reserve) {
    return reinterpret_cast<JsonArray &>(add(_debug_new JsonArray(name, reserve)));
}

JsonObject & JsonObjectMethods::addObject(const JsonString & name, size_t reserve) {
    return reinterpret_cast<JsonObject &>(add(_debug_new JsonObject(name, reserve)));
}

AbstractJsonValue * JsonObjectMethods::find(const JsonString & name) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            return *iterator;
        }
    }
    return nullptr;
}

AbstractJsonValue * JsonObjectMethods::find(const __FlashStringHelper * name) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            return *iterator;
        }
    }
    return nullptr;
}

AbstractJsonValue * JsonObjectMethods::find(const char * name) {
    auto vector = getVector();
    for (auto iterator = vector->begin(); iterator != vector->end(); ++iterator) {
        if (*(*iterator)->getName() == name) {
            return *iterator;
        }
    }
    return nullptr;
}
