/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Configuration.h"
#include "JsonConfigReader.h"

inline Configuration::~Configuration()
{
    clear();
    #if ESP32
        nvs_close(_handle);
        _handle = 0;
    #endif
}

inline void Configuration::clear()
{
    __LDBG_printf("params=%u", _params.size());
    _params.clear();
}

inline void Configuration::discard()
{
    __LDBG_printf("discard params=%u", _params.size());
    for(auto &parameter: _params) {
        ConfigurationHelper::deallocate(parameter);
    }
    _readAccess = 0;
}

inline bool Configuration::read()
{
    __LDBG_printf("params=%u", _params.size());
    clear();
    #if DEBUG_CONFIGURATION_GETHANDLE
        ConfigurationHelper::readHandles();
    #endif
    if (!_readParams()) {
        __LDBG_printf("readParams()=false");
        clear();
        return false;
    }
    return true;
}

inline const char *Configuration::getString(HandleType handle)
{
    __LDBG_printf("handle=%04x", handle);
    uint16_t offset;
    auto param = _findParam(ParameterType::STRING, handle, offset);
    if (param == _params.end()) {
        return emptyString.c_str();
    }
    auto result = param->getString(*this, offset);;
    if (!result) {
        __DBG_panic("handle=%04x string=%p", result);
    }
    return result;
}

inline const uint8_t *Configuration::getBinary(HandleType handle, size_type &length)
{
    __LDBG_printf("handle=%04x", handle);
    uint16_t offset;
    auto param = _findParam(ParameterType::BINARY, handle, offset);
    if (param == _params.end()) {
        length = 0;
        return nullptr;
    }
    return param->getBinary(*this, length, offset);
}

inline void Configuration::makeWriteable(ConfigurationParameter &param, size_type length)
{
    #if DEBUG_CONFIGURATION
        delay(1);
    #endif
    param._makeWriteable(*this, length);
}

inline char *Configuration::getWriteableString(HandleType handle, size_type maxLength)
{
    __LDBG_printf("handle=%04x max_len=%u", handle, maxLength);
    auto &param = getWritableParameter<char *>(handle, maxLength);
    return param._getParam().string();
}

inline void *Configuration::getWriteableBinary(HandleType handle, size_type length)
{
    __LDBG_printf("handle=%04x len=%u", handle, length);
    auto &param = getWritableParameter<void *>(handle, length);
    return param._getParam().data();
}

inline void Configuration::_setString(HandleType handle, const char *string, size_type length, size_type maxLength)
{
    __LDBG_printf("handle=%04x length=%u max_len=%u", handle, length, maxLength);
    if (maxLength != (size_type)~0U) {
        if (length >= maxLength - 1) {
            length = maxLength - 1;
        }
    }
    _setString(handle, string, length);
}

inline void Configuration::_setString(HandleType handle, const char *string, size_type length)
{
    __LDBG_printf("handle=%04x length=%u", handle, length);
    uint16_t offset;
    auto &param = _getOrCreateParam(ParameterType::STRING, handle, offset);
    param.setData(*this, (const uint8_t *)string, length);
}

inline void Configuration::setBinary(HandleType handle, const void *data, size_type length)
{
    __LDBG_printf("handle=%04x data=%p length=%u", handle, data, length);
    uint16_t offset;
    auto &param = _getOrCreateParam(ParameterType::BINARY, handle, offset);
    param.setData(*this, (const uint8_t *)data, length);
}

inline bool Configuration::isDirty() const
{
    for(auto &param: _params) {
        if (param.isWriteable()) {
            return true;
        }
    }
    return false;
}

inline bool Configuration::importJson(Stream &stream, HandleType *handles)
{
    JsonConfigReader reader(&stream, *this, handles);
    reader.initParser();
    return reader.parseStream();
}

#include "ConfigurationHelper.hpp"
#include "ConfigurationParameter.hpp"
#include "WriteableData.hpp"
