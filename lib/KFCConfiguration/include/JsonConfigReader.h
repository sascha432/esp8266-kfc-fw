/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include "Configuration.h"

class JsonConfigReader : public JsonBaseReader {
public:
    using HandleType = ConfigurationHelper::HandleType;
    using HandleVector = std::vector<HandleType>;
    using ParameterType = ConfigurationHelper::ParameterType;

    JsonConfigReader(Stream* stream, Configuration &config, HandleType *handles);
    JsonConfigReader(Configuration &config, HandleType *handles);

    virtual bool beginObject(bool isArray);
    virtual bool endObject();
    virtual bool processElement();
    virtual bool recoverableError(JsonErrorEnum_t errorType);

    HandleVector &getImportedHandles() {
        return _imported;
    }

private:
    long long stringToLl(const String &value) const;

private:

    static const HandleType INVALID_HANDLE = ~0;

    Configuration &_config;
    HandleType *_handles;
    uint16_t _handle;
    ParameterType _type;
    uint16_t _length;
    String _data;
    bool _isConfigObject;
    HandleVector _imported;
};
