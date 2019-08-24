/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonValue.h"

#if DEBUG_KFC_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AbstractJsonValue::~AbstractJsonValue() {
}

AbstractJsonValue & AbstractJsonValue::add(AbstractJsonValue * value) {
    return *value;
}

//void AbstractJsonValue::dump()
//{
//    static int valueCounter = 0, objectCounter = 0;
//    auto vector = getVector();
//    if (vector) {
//        objectCounter++;
//        printf("vector: %d, size %d (%d)\n", getType(), vector->size(), objectCounter);
//        for (auto &value : *vector) {
//            value->dump();
//        }
//    }
//    else {
//        valueCounter++;
//        printf("value: %d (total %d)\n", getType(), valueCounter);
//    }
//}

AbstractJsonValue::JsonVariantVector * AbstractJsonValue::getVector() {
    return nullptr;
}

const JsonString *AbstractJsonValue::getName() const {
    return nullptr;
}

bool AbstractJsonValue::hasName() const {
    //switch (getType()) {
    //case JSON_OBJECT:
    //case JSON_ARRAY:
    //case JSON_VARIANT:
    //    return true;
    //}
    return false;
}

bool AbstractJsonValue::hasChildName() const
{
    return false;
}

bool AbstractJsonValue::isObject() const {
    switch (getType()) {
    case JSON_OBJECT:
    case JSON_UNNAMED_OBJECT:
        return true;
    default:
        break;
    }
    return false;
}
