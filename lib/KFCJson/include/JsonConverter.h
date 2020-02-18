/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// Convert JSON string into writable object

#include <Arduino_compat.h>
#include "JsonBaseReader.h"
#include "JsonValue.h"

class JsonConverter : public JsonBaseReader {
public:
    typedef std::vector<AbstractJsonValue *> StackVector;
    typedef std::vector<JsonString> JsonStringVector;

    JsonConverter(Stream &stream);

    virtual bool beginObject(bool isArray);
    virtual bool endObject();
    virtual bool processElement();
    virtual bool recoverableError(JsonErrorEnum_t errorType);

    AbstractJsonValue *getRoot() const;
    void setIgnoreInvalid(bool ingnore);

    void addFilter(const JsonString &str);

private:
    AbstractJsonValue *_current;
    StackVector _stack;
    bool _ignoreInvalid;
    JsonStringVector _filter;
    uint16_t _filtered;
};

