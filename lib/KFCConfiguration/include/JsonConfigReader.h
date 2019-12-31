/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include "Configuration.h"

class JsonConfigReader : public JsonBaseReader {
public:
    JsonConfigReader(Stream* stream, Configuration &config, Configuration::Handle_t *handles);
    JsonConfigReader(Configuration &config, Configuration::Handle_t *handles);

    virtual bool beginObject(bool isArray);
    virtual bool endObject();
    virtual bool processElement();
    virtual bool recoverableError(JsonErrorEnum_t errorType);

private:
    long long stringToLl(const String &value) const;

private:
    static const uint16_t INVALID_HANDLE = ~0;

    Configuration &_config;
    Configuration::Handle_t *_handles;
    uint16_t _handle;
    ConfigurationParameter::TypeEnum_t _type;
    uint16_t _length;
    String _data;
    bool _isConfigObject;
};
