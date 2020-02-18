/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <limits>
#include "JsonTools.h"
#include "JsonVar.h"

class JsonBuffer;
class JsonConverter;
class JsonObjectMethods;

class AbstractJsonValue : public Printable {
public:
    typedef enum {
        JSON_VARIANT,
        JSON_UNNAMED_VARIANT,
        JSON_ARRAY,
        JSON_UNNAMED_ARRAY,
        JSON_OBJECT,
        JSON_UNNAMED_OBJECT,
    } JsonVariantEnum_t;

    typedef std::vector<AbstractJsonValue *> JsonVariantVector;
    typedef JsonVariantVector::iterator JsonVariantVectorIterator;

    virtual ~AbstractJsonValue();

    // length of converted JSON data
    virtual size_t length() const = 0;

    virtual JsonVariantEnum_t getType() const = 0;

    String toString() const {
        PrintString str;
        printTo(str);
        return str;
    }

    //void dump();

protected:
    friend JsonBuffer;
    friend JsonConverter;
    friend JsonObjectMethods;

    // add is public for JsonArray/JsonObject
    virtual AbstractJsonValue &add(AbstractJsonValue *value);

    virtual JsonVariantVector *getVector();
    virtual const JsonString *getName() const;
    virtual bool hasName() const;
    virtual bool hasChildName() const;
    virtual bool isObject() const;
};
