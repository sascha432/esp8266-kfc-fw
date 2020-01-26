/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>

class RestApiJsonReader : public JsonBaseReader {
public:
    RestApiJsonReader();

    virtual bool beginObject(bool isArray);
    virtual bool processElement();
    virtual bool recoverableError(JsonErrorEnum_t errorType);

private:
};
